/*
 *                                                                                                  geany_encoding=koi8-r
 * socket.c - socket IO (both client & server)
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#if defined CLIENT || defined DAEMON

#include "socket.h"
#include "term.h"
#include "usefull_macros.h"

#include <arpa/inet.h>  // inet_ntop
#include <limits.h>     // INT_xxx
#include <netdb.h>      // addrinfo
#include <pthread.h>
#include <signal.h>     // pthread_kill
#include <sys/prctl.h>  //prctl
#include <sys/wait.h>   // wait
#include <unistd.h>     // daemon

#define BUFLEN    (10240)
#define BUFLEN10  (1048576)
// Max amount of connections
#define BACKLOG   (30)

/**************** COMMON FUNCTIONS ****************/
/**
 * wait for answer from socket
 * @param sock - socket fd
 * @return 0 if data is absent, 1 in case of socket ready, -1 if socket closed or error occured
 */
static int waittoread(int sock){
    fd_set fds, efds;
    struct timeval timeout;
    int rc;
    timeout.tv_sec = 1; // wait not more than 1 second
    timeout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_ZERO(&efds);
    FD_SET(sock, &fds);
    FD_SET(sock, &efds);
    do{
        rc = select(sock+1, &fds, NULL, &efds, &timeout);
        if(rc < 0){
            if(errno != EINTR){
                putlog("Server not available");
                WARN("select()");
                return -1;
            }
            continue;
        }
        break;
    }while(1);
    if(FD_ISSET(sock, &efds)) return -1; // exception - socket closed
    if(FD_ISSET(sock, &fds))  return  1;
    return 0;
}

static uint8_t *findpar(uint8_t *str, char *par){
    size_t L = strlen(par);
    char *f = strstr((char*)str, par);
    if(!f) return NULL;
    f += L;
    if(*f != '=') return NULL;
    return (uint8_t*)(f + 1);
}
/**
 * get integer & double `parameter` value from string `str`, put value to `ret`
 * @return 1 if all OK
 */
static int getintpar(uint8_t *str, char *parameter, long *ret){
    long tmp;
    char *endptr;
    if(!(str = findpar(str, parameter))) return 0;
    tmp = strtol((char*)str, &endptr, 0);
    if(endptr == (char*)str || *str == 0 )
        return 0;
    if(ret) *ret = tmp;
    DBG("get par: %s = %ld", parameter, tmp);
    return 1;
}
#ifdef CLIENT
static int getdpar(uint8_t *str, char *parameter, double *ret){
    double tmp;
    char *endptr;
    if(!(str = findpar(str, parameter))) return 0;
    tmp = strtod((char*)str, &endptr);
    if(endptr == (char*)str || *str == 0)
        return 0;
    if(ret) *ret = tmp;
    DBG("get par: %s = %g", parameter, tmp);
    return 1;
}
#endif // CLIENT

/**************** CLIENT/SERVER FUNCTIONS ****************/
#ifdef DAEMON
static double min_dark_exp, dark_interval;
static imstorage *storedima = NULL;
static uint64_t imctr = 0; // image counter
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// setter for min_dark_exp, dark_interval
void set_darks(double exp, double dt){
    min_dark_exp = exp;
    dark_interval = dt;
}
static void clearimstorage(){
    if(!storedima) return;
    FREE(storedima->imname);
    FREE(storedima->subframe);
    FREE(storedima->imdata);
    FREE(storedima);
}
static imstorage *copyima(imstorage *im){
    FNAME();
    clearimstorage();
    if(!im || !im->imdata) return NULL;
    storedima = MALLOC(imstorage, 1);
    #define CLR() do{clearimstorage(); return NULL;}while(0)
    if(!memcpy(storedima, im, sizeof(imstorage))) CLR();
    if(im->imname){
        if(!(storedima->imname = strdup(im->imname))) CLR();
    }
    if(im->subframe){
        storedima->subframe = MALLOC(imsubframe, 1);
        if(!memcpy(storedima->subframe, im->subframe, sizeof(imsubframe))) CLR();
    }
    if(im->imdata){
        size_t S = im->W*im->H*sizeof(uint16_t);
        if(!(storedima->imdata = malloc(S))) CLR();
        if(!memcpy(storedima->imdata, im->imdata, S)) CLR();
    }else{
        WARNX("Where is imdata?");
        CLR();
    }
    #undef CLR
    DBG("ALL ok");
    return storedima;
}

static int addwebhdr(char *buf, size_t buflen, char *conttype, size_t contlen){
    return snprintf(buf, buflen,
        "HTTP/2.0 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Content-type: %s\r\nContent-Length: %zd\r\n\r\n", conttype, contlen);
}

int send_ima(int sock, int webquery){
    uint8_t buf[BUFLEN10], *bptr = buf, obuff[BUFLEN];
    int Len, rest = BUFLEN10, imS = storedima->W * storedima->H * sizeof(uint16_t);
    #define PUT(key, val) do{Len = snprintf((char*)bptr, rest, "%s=%i\n", key, (int)storedima->val); \
                if(Len > 0){rest -= Len; bptr += Len;}}while(0)
    PUT("binning", binning);
    if(storedima->binning == 0xff){
        PUT("subX", subframe->Xstart);
        PUT("subY", subframe->Xstart);
        PUT("subS", subframe->size);
    }
    Len = snprintf((char*)bptr, rest, "%s=%g\n", "exptime", storedima->exptime);
    if(Len > 0){rest -= Len; bptr += Len;}
    PUT("imtype", imtype);
    PUT("imW", W);
    PUT("imH", H);
    PUT("exposetime", exposetime);
    Len = snprintf((char*)bptr, rest, "imdata=");
    if(Len){rest -= Len; bptr += Len;}
    if(rest < imS){
        red("rest = %d, need %d\n", rest, imS);
        return 0; // not enough memory - HOW???
    }
    if(!memcpy(bptr, storedima->imdata, imS)){
        WARN("memcpy()");
        return 0;
    }
    rest -= imS;
    // send data
    int send = BUFLEN10 - rest;
    // OK buffer ready, prepare to send it
    if(webquery){
        Len = addwebhdr((char*)obuff, BUFLEN, "multipart/form-data", send);
        if(Len < 0){
            WARN("sprintf()");
            return 0;
        }
        if(Len != write(sock, obuff, Len)){
            WARN("write");
            return 0;
        }
        DBG("%s", obuff);
    }
    red("send %d bytes\n", send);
    if(send != write(sock, buf, send)){
        WARN("write()");
        return 0;
    }
    putlog("image sent to client");
    return 1;
}

// search a first word after needle without spaces
char* stringscan(char *str, char *needle){
    char *a, *e;
    char *end = str + strlen(str);
    a = strstr(str, needle);
    if(!a) return NULL;
    a += strlen(needle);
    while (a < end && (*a == ' ' || *a == '\r' || *a == '\t' || *a == '\r')) a++;
    if(a >= end) return NULL;
    e = strchr(a, ' ');
    if(e) *e = 0;
    return a;
}

void *handle_socket(void *asock){
    FNAME();
    uint64_t locctr = 0;
    int sock = *((int*)asock);
    int webquery = 0; // whether query is web or regular
    char buff[BUFLEN];
    ssize_t _read;
    while(1){
        int rd = waittoread(sock);
        if(rd < 0){
            putlog("Disconnected");
            break;
        }
        if(!rd){ // no data incoming
            pthread_mutex_lock(&mutex);
            if(imctr != locctr){
                red("Send image, imctr = %ld, locctr = %ld\n", imctr, locctr);
                if(send_ima(sock, webquery)){
                    locctr = imctr;
                    if(webquery){
                        pthread_mutex_unlock(&mutex);
                        break; // end of transmission
                    }
                }
            }
            pthread_mutex_unlock(&mutex);
            continue;
        }
        _read = read(sock, buff, BUFLEN);
        if(_read < 1){ // error or disconnect
            putlog("Client disconnected");
            DBG("Nothing to read from fd %d (ret: %zd)", sock, _read);
            break;
        }
        DBG("Got %zd bytes", _read);
        // add trailing zero to be on the safe side
        buff[_read] = 0;
        // now we should check what do user want
        char *got, *found = buff;
        if((got = stringscan(buff, "GET")) || (got = stringscan(buff, "POST"))){ // web query
            webquery = 1;
            char *slash = strchr(got, '/');
            if(slash) found = slash + 1;
            // web query have format GET /some.resource
        }
        // here we can process user data
        printf("user send: %s\n", found);
        long htr;
        // double dd;
        //if(getdpar((uint8_t*)found, "exptime", &dd)) printf("exptime: %g\n", dd);
        if(getintpar((uint8_t*)found, "heater", &htr)){
            putlog("got command: heater=%ld", htr);
            if(htr == 0) heater_off();
            else heater_on();
            int Len = addwebhdr(buff, BUFLEN, "text/html", 12);
            if(Len > 0){
                Len += snprintf(buff+Len, BUFLEN, "HEATER %s\r\n", htr ? "ON " : "OFF");
                write(sock, buff, Len);
            }
            break; // disconnect after command receiving
        }
    }
    close(sock);
    //DBG("closed");
    pthread_exit(NULL);
    return NULL;
}

void *server(void *asock){
    int sock = *((int*)asock);
    if(listen(sock, BACKLOG) == -1){
        WARN("listen");
        return NULL;
    }
    while(1){
        socklen_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in their_addr;
        int newsock;
        int rd = waittoread(sock);
        if(rd < 0){
            putlog("Socket error");
            return NULL;
        }
        if(!rd) continue;
        red("Got connection\n");
        newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
        if(newsock <= 0){
            WARN("accept()");
            continue;
        }
        putlog("Got connection from %s\n", inet_ntoa(their_addr.sin_addr));
        pthread_t handler_thread;
        if(pthread_create(&handler_thread, NULL, handle_socket, (void*) &newsock))
            WARN("pthread_create()");
        else{
            DBG("Thread created, detouch");
            pthread_detach(handler_thread); // don't care about thread state
        }
    }
}

static void daemon_(imstorage *img, int sock){
    FNAME();
    if(sock < 0) return;
    pthread_t sock_thread;
    static double lastDT = 0.; // last time dark was taken
    if(pthread_create(&sock_thread, NULL, server, (void*) &sock))
        ERR("pthread_create()");
    int errcntr = 0;
    do{
        if(pthread_kill(sock_thread, 0) == ESRCH){ // died
            WARNX("Sockets thread died");
            putlog("Sockets thread died");
            pthread_join(sock_thread, NULL);
            if(pthread_create(&sock_thread, NULL, server, (void*) &sock))
                ERR("pthread_create()");
        }
        if(exp_calculated > 0.) img->exptime = exp_calculated;
        if(img->imtype != IMTYPE_AUTODARK){ // check for darks
            if(img->imtype == IMTYPE_DARK){
                putlog("First light frame after dark");
                img->imtype = IMTYPE_LIGHT; // last was dark
            }
            else if(img->exptime > min_dark_exp){ // need to store dark frame?
                putlog("exptime > %g; dtime()=%.1f, lastDT = %.1f", min_dark_exp, dtime(), lastDT);
                if(dtime() - lastDT > dark_interval){
                    putlog("Take dark image");
                    lastDT = dtime();
                    img->imtype = IMTYPE_DARK;
                }
            }
        }
        if(start_exposition(img, NULL)){
            putlog("Error starting exposition, try later");
            WARNX(_("Error starting exposition, try later"));
            ++errcntr;
        }else{
            FREE(img->imdata);
            if(!get_imdata(img)){
                ++errcntr;
                putlog("Error image transfer");
                WARNX(_("Error image transfer"));
            }else{
                errcntr = 0;
                pthread_mutex_lock(&mutex);
                if(copyima(img)){
                    ++imctr;
            if(img->imtype != IMTYPE_DARK)
                    save_histo(NULL, img); // calculate next optimal exposition
                }
                pthread_mutex_unlock(&mutex);
            }
        }
        if(errcntr >= 33){
            putlog("Unrecoverable error, errcntr=%d. Exit", errcntr);
            ERRX(_("Unrecoverable error"));
        }
    }while(1);
}
#endif

#ifdef CLIENT
static imstorage *get_imstorage(imstorage *img, uint8_t *buf, size_t L){
    imsubframe F;
    long i; double d;
    img->subframe = NULL;
    if(getintpar(buf, "binning", &i)){
        img->binning = i;
        DBG("bin: %d", img->binning);
        if(i == 0xff){ // subframe
            if(getintpar(buf, "subX", &i)) F.Xstart = i;
            if(getintpar(buf, "subY", &i)) F.Ystart = i;
            if(getintpar(buf, "subS", &i)) F.size = i;
            img->subframe = &F;
        }
    }
    if(getdpar(buf, "exptime", &d)) img->exptime = d;
    if(getintpar(buf, "imtype", &i)) img->imtype = i;
    if(getintpar(buf, "imW", &i)) img->W = i;
    if(getintpar(buf, "imH", &i)) img->H = i;
    if(getintpar(buf, "exposetime", &i)) img->exposetime = i;
    uint8_t *par = findpar(buf, "imdata");
    if(par){
        img->imdata = (uint16_t*)par;
        size_t datasz = img->W * img->H * sizeof(uint16_t);
        if(datasz > L - (par - buf)) return NULL; // buffer too small for given image
        DBG("1st pix: %u; W=%zd, H=%zd", *img->imdata, img->W, img->H);
    }else return NULL;
    return img;
}

static void client_(imstorage *img, int sock){
    FNAME();
    if(sock < 0) return;
    size_t Bufsiz = BUFLEN10;
    uint8_t *recvBuff = MALLOC(uint8_t, Bufsiz);
    time_t wd_time = time(NULL); // watchdog time
    time_t wd_period = get_wd_period(); // watchdog period: about 3minutes + 2*max_exptime
    while(1){
        // check watchdog
        if(time(NULL) - wd_time > wd_period){
            putlog("Watchdog triggered, exit!");
            return;
        }
        int rd = waittoread(sock);
        if(rd < 0){
            putlog("Server disconnected");
            break;
        }
        if(!rd) continue;
        size_t offset = 0;
        do{
            if(offset >= Bufsiz){
                Bufsiz += 1024;
                recvBuff = realloc(recvBuff, Bufsiz);
                if(!recvBuff){
                    WARN("realloc()");
                    return;
                }
                DBG("Buffer reallocated, new size: %zd\n", Bufsiz);
            }
            ssize_t n = read(sock, &recvBuff[offset], Bufsiz - offset);
            if(!n){
                putlog("Socket closed");
                break;
            }
            if(n < 0){
                putlog("error in read()");
                WARN("read");
                return;
            }
            offset += n;
        }while(waittoread(sock));
        if(!offset){
            putlog("Socket closed");
            WARN("Socket closed\n");
            return;
        }
        DBG("read %zd bytes\n", offset);
        wd_time = time(NULL); // refresh watchdog - socket OK
        if(get_imstorage(img, recvBuff, offset)){
            if(store_image(img)){
                putlog("Error storing image");
                WARNX(_("Error storing image"));
            }else{
                putlog("Image saved");
            }
        }
        if(img->once) break;
    }
}
#endif

/**
 * Run daemon service
 */
void daemonize(imstorage *img, char *hostname, char *port){
    FNAME();
    int sock = -1;
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    DBG("connect to %s:%s", hostname, port);
    if(getaddrinfo(hostname, port, &hints, &res) != 0){
        ERR("getaddrinfo");
    }
    struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
    DBG("canonname: %s, port: %u, addr: %s\n", res->ai_canonname, ntohs(ia->sin_port), str);
    putlog("connect to port %s", port);
    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            WARN("socket");
            continue;
        }
#ifdef DAEMON
            int reuseaddr = 1;
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
                ERR("setsockopt");
            }
            if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
                close(sock);
                WARN("bind");
                continue;
            }
#else
            if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
                WARN("connect()");
                close(sock);
                continue;
            }
#endif
        break; // if we get here, we have a successfull connection
    }
    if(p == NULL){
        // looped off the end of the list with no successful bind
        putlog("failed to bind socket");
        ERRX("failed to bind socket");
    }
    freeaddrinfo(res);
#ifdef CLIENT
    client_(img, sock);
#else
    daemon_(img, sock);
#endif
    putlog("Close socket");
    close(sock);
    signals(0);
}

#endif // CLIENT || DAEMON
