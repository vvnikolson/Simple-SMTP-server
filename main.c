#include "includings.h"
#include "database.h"
#include "replies.h"
#include <signal.h>
#include <syslog.h>
#include <ctype.h>
/*  Мультиплексирование ввода-вывода на основе epoll() */

#define DEFPORT 12344
#define SIZE_ADDJUST 10
#define BUF_SIZE 2048
#define LOGGING 1

// Возможные состояния клиента
typedef enum {
    CONNECTION_ESTABLISHED, // После установки соединения
    SERVER_READY,			// после HELO
    SENDER_SPECIFIED,		// После MAIL FROM:
    RECIPIENT_SPECIFIED,	// После RCPT TO:
    DATA_ENTERING			// После DATA
} client_state;

sqlite3* pDB;				// Указатель на БД
// Структура, содержащая данные клиента
struct client_desc {
    int fd;					// Число, файловый дескриптор клиента
    int sender_id;			// Идентификатор отправителя в БД
    struct {
        int count;			// Количество получателей
        int *rcpt_ids;		// Идентификаторы получателей в БД
    } rcpt_to;
    char *msg;				// Указатель на тело сообщения
    client_state state;		// Состояние клиента
};

void SMTP_respond(struct client_desc* client_desc, char* request);
void get_mail_content(struct client_desc* c_desc, char* data, int len);
void rset_user(struct client_desc *c_desc);
void end_message(struct client_desc* c_desc);
int add_to_msg(char** msg, char* data, int len);
void quit_user(struct client_desc *c_desc);
void send_respond(int fd, reply respond);



int main(int argc, char **argv) {
    int efd;
    openlog("[epoll SMTP server]", 0, LOG_MAIL);
    // Отключение реакции на SIGPIPE
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    pDB = initialize_database();
    int listen_socket;
    struct sockaddr_in master_addr;
    unsigned short portnum = DEFPORT;
    int count = 2048;
    char buf[BUF_SIZE];
    char hostname[50];
    gethostname(hostname, 50);
    printf("\nStarting Simple Server... %s\n\n", hostname);

    //Создание сокета
    listen_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    //Задание имени сокету
    master_addr.sin_family = AF_INET;					// семейство адресов
    master_addr.sin_port = htons(portnum);				// Входящий порт
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// Принимаем любой адресс
    if (bind(listen_socket, (struct sockaddr *) &master_addr, sizeof(master_addr))) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    //Перевод в прослушивание
    if (listen(listen_socket, SOMAXCONN)) {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening port %d for incoming connections...\n", portnum);
    //Подготавливаем дескриптор epoll
    efd = epoll_create1(0);
    if (efd < 0) {
        perror("epoll_create1() failed");
        exit(EXIT_FAILURE);
    }
    struct epoll_event ev, *events;
    events = (struct epoll_event*) calloc(count, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.ptr = malloc(sizeof(struct client_desc));
    ((struct client_desc*) ev.data.ptr)->fd = listen_socket;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listen_socket, &ev)) {
        perror("epoll_ctl() failed");
        exit(EXIT_FAILURE);
    }
    // Добавляем события с STDIN
    ev.events = EPOLLIN;
    ev.data.ptr = malloc(sizeof(struct client_desc));
    ((struct client_desc*)ev.data.ptr)->fd = 0;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, 0, &ev)) {
        perror("epoll_ctl() failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Ждём события (приход данных в буфер) на сокетах
        int ready;
        ready = epoll_wait(efd, events, count, -1);
        if (ready == -1 && errno != EINTR) {
            perror("epoll_wait() failed");
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i < ready; i++) {
            struct client_desc *c_event = events[i].data.ptr;
            // Событие с STDIN, ввод в консоль
            if (c_event->fd == 0) {
                memset(buf,0, sizeof(buf));
                if (read(0, buf, sizeof(buf)) > 1) {
                    buf[strcspn(buf, "\r\n")] = 0; // удаление спецсимволов конце строки
                    int user_id = get_user_id(pDB, buf);
                    if (user_id>0)
                        print_user_info(pDB, user_id);
                    else
                        printf("Address not found");
                }
                // Обработка master сокета, новый клиент
            } else if (c_event->fd == listen_socket) {
                int sock = accept(listen_socket, NULL, NULL); // выделение нового файлового дескриптора для клиента
                printf ("[ New client accepted, socket num: %d ]\n", sock);
                if(LOGGING)
                    syslog(LOG_INFO, "[ New client accepted, socket num: %d ]\n", sock);
                ev.events = EPOLLIN; // ожидаемое событие -- ввод данных
                // Инициализация мтруктуры с данными клиента
                ev.data.ptr = malloc(sizeof(struct client_desc));
                ((struct client_desc *) ev.data.ptr)->fd = sock;
                ((struct client_desc *) ev.data.ptr)->state = CONNECTION_ESTABLISHED;
                ((struct client_desc *) ev.data.ptr)->msg = NULL;
                send_respond(sock, reply_codes[3]);
                epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev); // Привязать созданный дескриптор к событию epoll
                //if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
                //    perror("failed to set non-blocking mode");
                //}
            }
            else { // Пришли данные от уже подключённого клиента
                memset(buf, 0, sizeof(buf));
                int msg_len = recv(c_event->fd, buf, sizeof(buf), 0);

                if (msg_len < 0) {
                    printf("Recv error: %s\n", strerror(errno));
                    quit_user(c_event);
                    continue;
                }
                if (msg_len == 0) { // клиент оборвал соединение
                    quit_user(c_event);
                    continue;
                }
                printf("[->]len:%i\t\t%s", msg_len, buf);
                if(LOGGING)
                    syslog(LOG_INFO, "[->] len:%i    %s", msg_len, buf);
                if(events[i].events & EPOLLIN) {
                    if(c_event->state != DATA_ENTERING) { //Если клиент не заполняет тело письма
                        if (LOGGING)					// интерпретируем данные как SMTP команды
                            syslog(LOG_INFO, "[<-] [Socket %i]    %.4s", c_event->fd, buf);
                        SMTP_respond(c_event, buf);
                    } else { // иначе сохраняем всё в тело письма
                        get_mail_content(c_event, buf, msg_len);
                    }
                }
            }
        }
    }
}

char* get_address(const char* request){
    // найти адресс в строке
    // адресс должен быть обрамлён "<" ">"
    char *s_a, *s_b, *address;
    s_a = strchr(request, '<');
    s_b = strchr(request, '>');
    if (!(s_a && s_b) || (s_b - s_a)==1) {
        return NULL;
    }
    address = calloc(sizeof(char), s_b - s_a - 1);
    stpncpy(address, s_a + 1, s_b - s_a - 1);

    return address;
}

void SMTP_respond(struct client_desc* c_desc, char* request) {
    // Перевести все буквы в нижний регистр
    char request_low[10];
    strncpy(request_low, request, 10);
    for(int i = 0; i < 10 && request[i]; i++){
        request_low[i] = tolower(request[i]);
    }
    if (strncmp(request_low, "helo", 4) == 0) { // HELO
        // Инициализируем начальным состоянием
		c_desc->state = SERVER_READY;
        c_desc->rcpt_to.rcpt_ids = NULL;
        c_desc->rcpt_to.count = 0;
        c_desc->sender_id = -1;
        send_respond(c_desc->fd, reply_codes[5]);
    } else if (strncmp(request_low, "mail from:", 10) == 0) { // MAIL FROM
        if (c_desc->state == SERVER_READY) { // Проверка правильной последовательности команд
            char *address = get_address(request);
            if (!address) { // В запросе нет адреса
                send_respond(c_desc->fd, reply_codes[10]);
                return;
            }
			// поиск адреса в базе, если нет, то добавляем
            int id = get_user_id(pDB, address); 
            if (id >= 0) {
                c_desc->sender_id = id;
            } else {
                c_desc->sender_id = add_user(pDB, address);
            }
            c_desc->state = SENDER_SPECIFIED;
            free(address);
            send_respond(c_desc->fd, reply_codes[5]);
        } else {
            send_respond(c_desc->fd, reply_codes[12]);
            return;
        }
    } else if (strncmp(request_low, "rcpt to:", 8) == 0) { // RCPT TO
        if (c_desc->state == SENDER_SPECIFIED || c_desc->state == RECIPIENT_SPECIFIED) {
            char *address = get_address(request);
            if (!address) { // В запросе нет адреса
                send_respond(c_desc->fd, reply_codes[10]);
                return;
            }
            int id = get_user_id(pDB, address);
            free(address);
            if(id < 0) { // если адрес не найден
                //id = add_user(pDB, address);
				//то говорим что такого нет (not local)
                send_respond(c_desc->fd, reply_codes[14]);
                return;
            }
			// выделение памяти под получателей по 10 штук
            if (c_desc->rcpt_to.rcpt_ids == NULL) { 
                c_desc->rcpt_to.rcpt_ids = calloc(sizeof(int), SIZE_ADDJUST);
            } else if (c_desc->rcpt_to.count%SIZE_ADDJUST == 0) { // если не хватает
				// выделяем ещё 10 и проверяем realloc
                int* tmp = realloc(c_desc->rcpt_to.rcpt_ids,
                                   sizeof(int)*(c_desc->rcpt_to.count + SIZE_ADDJUST + 1));
                if (tmp==NULL) {
                    if(LOGGING)
                        syslog(LOG_ALERT,"Cannot realloc memory");
                    send_respond(c_desc->fd, reply_codes[8]);
                    return;
                } else {
                    c_desc->rcpt_to.rcpt_ids = tmp;
                }
            }
            c_desc->rcpt_to.rcpt_ids[c_desc->rcpt_to.count++] = id;
            c_desc->state = RECIPIENT_SPECIFIED;
            send_respond(c_desc->fd, reply_codes[5]);
        } else {
            send_respond(c_desc->fd, reply_codes[12]);
        }
    } else if (strncmp(request_low, "data", 4) == 0) { // DATA
        if (c_desc->state == RECIPIENT_SPECIFIED) {
            send_respond(c_desc->fd, reply_codes[6]);
            c_desc->state = DATA_ENTERING; // переключение в состояние ввода тела письма
        } else {
            send_respond(c_desc->fd, reply_codes[12]);
        }
    } else if (strncmp(request_low, "noop", 4) == 0) { // NOOP
        send_respond(c_desc->fd, reply_codes[5]);
    } else if (strncmp(request_low, "quit", 4) == 0) { // QUIT
        send_respond(c_desc->fd, reply_codes[4]);
        quit_user(c_desc);
    } else if (strncmp(request_low, "rset", 4) == 0) { // RSET
        send_respond(c_desc->fd, reply_codes[5]); // сбросить всё память и указатели
        rset_user(c_desc);
        c_desc->state = SERVER_READY;
    } else { // На всё остальное один ответ -- команда не реализована
        send_respond(c_desc->fd, reply_codes[11]);
    }
}

void rset_user(struct client_desc *c_desc) {
    // Освободить память (тело сообщения и список получателей)
    free(c_desc->rcpt_to.rcpt_ids);
    free(c_desc->msg);
}

void quit_user(struct client_desc *c_desc) {
    // отключить клиента, закрыть файловый дескриптор
    if (c_desc->state != CONNECTION_ESTABLISHED) // проверка состояния
		// до ввода HELO структура не инициализирована
		// Освобождение ещё не выделенной памяти -- порча кучи
        rset_user(c_desc);
    close(c_desc->fd);
    free(c_desc);
}

int add_to_msg(char** msg, char* data, int len) {
    // добавить часть новую часть сообщения data длиной len к уже имеющимся данным msg
    // Вернуть -1 в случае неудачи realloc
    if (*msg == NULL) {
        *msg = calloc(sizeof(char), len+1);
        strncat(*msg, data, len);
    } else {
        char* tmp = realloc(*msg, sizeof(char)*(strlen(*msg) + len)+1);
        if(tmp){
            *msg = tmp;
            strncat(*msg, data, len);
        } else
            return -1;
    }
    return 0;
}
void end_message(struct client_desc* c_desc) {
    // Закончить ввод сообщения: сбросить состояние клиента и сохранить письмо
    c_desc->state = SERVER_READY;
    send_respond(c_desc->fd, reply_codes[5]);
    save_mail(pDB, c_desc->msg, c_desc->sender_id, c_desc->rcpt_to.rcpt_ids, c_desc->rcpt_to.count);
}

void get_mail_content(struct client_desc* c_desc, char* data, int len) {

    if (len == 3 && strncmp(data, ".\r\n", 3) == 0) { //если новые данные не содержат ничего, кроме точки на новой строке
        end_message(c_desc); // закончить ввод сообщения
        return;
    }
    if(len >= 5 && strncmp(&data[len-5], "\r\n.\r\n", 5) == 0) {
        // если пришедшие данные заканчиваются одинокой точко на новой строке
        data[len-5] = 0; // удаляем точку и заканчиваем ввод сообщения
        if(add_to_msg(&c_desc->msg, data, len) == 0) {
            end_message(c_desc);
        } else {
            if(LOGGING)
                syslog(LOG_ALERT, "[Client soc %i] cannot realloc memory", c_desc->fd);
            send_respond(c_desc->fd, reply_codes[8]);
        }
        return;
    } else { // иначе добавляем новую часть к имеющецся и ждём новую часть тела сообщения
        if(add_to_msg(&c_desc->msg, data, len) != 0) {
            if(LOGGING)
                syslog(LOG_ALERT, "[Client soc %i] cannot realloc memory", c_desc->fd);
            send_respond(c_desc->fd, reply_codes[8]);
        }
        return;
    }
}

void send_respond(int fd, reply respond) {
    // отправить клиенту сообшение

    send(fd, respond.str, respond.len, 0);
    printf("[<-] %i\n", respond.code);
    if (LOGGING) {
        /*
        switch (respond.code) {
            case 200:;
            case 220:;
            case 250:;
        }
        */
        syslog(LOG_INFO, "[->] [Socket %i]    %i", fd, respond.code);
    }
}