//
// Created by vvnikolson on 4/28/19.
//

#include "database.h"
#define max(x,y) ((x) >= (y)) ? (x) : (y)

void create_tables(sqlite3 *pDB){
	// Создание таблиц, если их не сущетвует в указанной базе данных
    int rc;
    char *err_msg = 0;
	// Разрешить использование внешних ключей, на всякий случай
    rc = sqlite3_exec(pDB, "PRAGMA foreign_keys = ON;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(pDB);
    }
    char *sql_users = "CREATE TABLE IF NOT EXISTS 'Users' ('user_id' INTEGER PRIMARY KEY, 'username' VARCHAR(255) NOT NULL UNIQUE);";
    rc = sqlite3_exec(pDB, sql_users, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(pDB);
    }
    char *sql_mails = "CREATE TABLE IF NOT EXISTS 'Mails' ('mail_id' INTEGER PRIMARY KEY, 'timestamp' TIME, 'text' TEXT NOT NULL);";
    rc = sqlite3_exec(pDB, sql_mails, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(pDB);
    }

    char *sql_junc = "CREATE TABLE IF NOT EXISTS 'Junction' ('mail_id' INT NOT NULL, 'sender_id' INT NOT NULL, 'recipient_id' INT NOT NULL, FOREIGN KEY('sender_id') REFERENCES Users(user_id), FOREIGN KEY('recipient_id') REFERENCES Users(user_id), FOREIGN KEY('mail_id') REFERENCES Mails(mail_id));";
    rc = sqlite3_exec(pDB, sql_junc, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(pDB);
    }
}


sqlite3* initialize_database(){
	// Инициализация внутренних структур библиотеки и базы данных
    sqlite3_initialize();
    sqlite3 *pDB = NULL;
    int rc;
    rc = sqlite3_open("test.db", &pDB);
    if(rc != SQLITE_OK) {
        sqlite3_close(pDB);
    }

    create_tables(pDB);
    return pDB;
}

int add_user(sqlite3 *pDB, const char* username){
	// Добавление нового пользователя в базу данных
	// Возвращает его идентификатор в базе данных, -1 в случае ошибки
    sqlite3_stmt *res;
	// Подготовить SQL запрос к выполнению, 
	//(?) -- место для последующей вставки аргументов
    int rc = sqlite3_prepare_v2(pDB,"INSERT INTO Users(username) VALUES(?)", -1, &res, 0);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
	// Вставить строку в запрос вместо (?)
    rc = sqlite3_bind_text(res, 1, username, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
	// Выполник SQL запро
    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    sqlite3_finalize(res);
    return sqlite3_last_insert_rowid(pDB);
}

int get_user_id(sqlite3 *pDB, const char* username) {
	// Поиск пользователя в базе данных по его адресу
	// Возвращает его идентификатор и -1 если таковой отсутсвует
    sqlite3_stmt *res;
    if(strlen(username) > 19){
        printf("asa");
    }
    int rc = sqlite3_prepare_v2(pDB, "SELECT user_id FROM Users WHERE username=(?);", -1, &res, 0);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    rc = sqlite3_bind_text(res, 1, username, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    int aa;
    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        printf("error: %s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    if (rc == SQLITE_DONE){
        sqlite3_finalize(res);
        return -1;
    } else {
        aa = sqlite3_column_int(res, 0);
    }
    sqlite3_finalize(res);
    return aa;
}

int save_mail(sqlite3 *pDB, const char* mail_data, int mail_from, int* rcpt_to, int cnt) {
	/*
	Сохранить письмо в базу данных
		*pDB		-- Указатель на базу данных куда сохранять
		*mail_data	-- Указатель на тело письма
		mail_from	-- Идентификатор отправителя
		*rcpt_to	-- Идентификаторы получателей письма
		cnt			-- Количество получателей 
	*/
    sqlite3_stmt *res;
    int timestamp = time(NULL);
    int rc = sqlite3_prepare_v2(pDB, "INSERT INTO Mails(timestamp, text) VALUES(?, ?);", -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    rc = sqlite3_bind_int(res, 1, timestamp);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    rc = sqlite3_bind_text(res, 2, mail_data, -1,SQLITE_TRANSIENT);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return -1;
    }
    sqlite3_finalize(res);
    int mail_id =  sqlite3_last_insert_rowid(pDB); // Идентификатор письма в БД

	// Добавление записей в Соединительную (Junction) таблицу
    sqlite3_stmt *jres;
	// По умолчанию, все изменения сразу сохраняются в БД после выполнения запроса
	// Здесь идея -- выполнить несколько запросов на добавление записей в таблицу
	// но сохранять изменения только когда выполняться все
    sqlite3_exec(pDB, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    rc = sqlite3_prepare_v2(pDB,
            "INSERT INTO Junction(mail_id, sender_id, recipient_id) VALUES(?, ?, ?);",
            -1, &jres, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(jres);
        return -1;
    }
	// Для каждого получателя выполняем запрос
    for(int i=0; i <cnt; i++) {
        sqlite3_reset(jres); // Сбросить привязанные переменные к запросу
        rc = sqlite3_bind_int(jres, 1, mail_id);
        if (rc != SQLITE_OK ) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
            sqlite3_finalize(jres);
            return -1;
        }
        rc = sqlite3_bind_int(jres, 2, mail_from);
        if (rc != SQLITE_OK ) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
            sqlite3_finalize(jres);
            return -1;
        }
        rc = sqlite3_bind_int(jres, 3, rcpt_to[i]);
        if (rc != SQLITE_OK ) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
            sqlite3_finalize(jres);
            return -1;
        }
        rc = sqlite3_step(jres);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
            sqlite3_finalize(jres);
            return -1;
        }
    }
    sqlite3_exec(pDB, "COMMIT;", NULL, NULL, NULL); // Сохранить изменения
    sqlite3_finalize(jres);
    return 0;
}

void print_user_info(sqlite3 *pDB, int id) {
	// напечатать в stdout статистику об отправителях и получателях для заданного адреса
    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(pDB,
            "SELECT r.username, COUNT(*) FROM Junction AS j INNER JOIN Users AS r ON r.user_id = j.recipient_id where j.sender_id = (?) group by 1 order by 2 DESC;",
            -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return;
    }
    rc = sqlite3_bind_int(res, 1, id);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return;
    }
	int sentToSize = 10; // Начальный размер массива получателей 
    char **sentTo = malloc(sentToSize*sizeof(unsigned char*));// Массив указателей на получателей
    int *sentToMailsCount = malloc(sentToSize*sizeof(int)); // количество писем для соответсвующего адресата

    int sentToCount; // количество получателей
    for (sentToCount=0; (rc = sqlite3_step(res)) == SQLITE_ROW; sentToCount++) {
        if(sentToCount >= sentToSize) { // Если для очередного адреса не хватает памяти
            sentToSize *=2; // то увеличить размер в 2 раза
            int *tmp = realloc(sentToMailsCount, sentToSize * sizeof(int));
            if(tmp) // Проверка realloc
                sentToMailsCount = tmp;
            else {
                printf("Cannot realloc memory {sentToMailsCount}");
                goto free_resources_sentTo; // Если всё плохо - прыгаем к концу и освобождаем то, что успели выделить
            }
            char** tmpc = realloc(sentTo, sentToSize* sizeof(unsigned char*));
            if(tmpc)
                sentTo = tmpc;
            else {
                printf("Cannot realloc memory {sentTo}");
                goto free_resources_sentTo;
            }
        }
        sentTo[sentToCount] = strdup((char*) sqlite3_column_text(res, 0)); //выделить память и скопировать туда очередной адрес из запроса
        sentToMailsCount[sentToCount] = sqlite3_column_int(res, 1); // Сохранить количество писем из запроса
    }
    sqlite3_finalize(res);
    printf("---------------------------------------\n");
    //------------------------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------
    
	// Теперь всё то же самое, но для списка отправителей
	rc = sqlite3_prepare_v2(pDB,
             "SELECT s.username, COUNT(*) FROM Junction AS j INNER JOIN Users AS s ON s.user_id = j.sender_id where j.recipient_id = (?) group by 1 order by 2 DESC;",
             -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return;
    }
    rc = sqlite3_bind_int(res, 1, id);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(pDB));
        sqlite3_finalize(res);
        return;
    }
    char **receivedFrom;
    int receivedFromSize = 10;
    receivedFrom = malloc(receivedFromSize* sizeof(unsigned char*));
    int *receivedFromMailsCount = malloc(receivedFromSize* sizeof(int));

    int receivedFromCount;
    for(receivedFromCount=0;(rc = sqlite3_step(res)) == SQLITE_ROW; receivedFromCount++) {
        if(receivedFromCount >= receivedFromSize) {
            receivedFromSize *=2;
            int *tmp = realloc(receivedFromMailsCount, receivedFromSize * sizeof(int));
            if(tmp)
                receivedFromMailsCount = tmp;
            else {
                printf("Cannot realloc memory {receivedFromMailsCount}");
                goto free_resources_receivedFrom;
            }
            char** tmpc = realloc(receivedFrom, receivedFromSize* sizeof(unsigned char*));
            if(tmpc)
                sentTo = tmpc;
            else {
                printf("Cannot realloc memory {receivedFrom}");
                goto free_resources_receivedFrom;
            }
        }
        receivedFrom[receivedFromCount] = strdup((char*) sqlite3_column_text(res, 0));
        receivedFromMailsCount[receivedFromCount] = sqlite3_column_int(res, 1);
    }
    sqlite3_finalize(res);
    //----------------------------------------------------------------
	// Формирование таблицы из полученных данных и вывод в STDOUT
    printf("%30s %35s\n","sent to", "received from");
    int mama = max(receivedFromCount,sentToCount);
    for(int i=0; i < mama; i++) {
        if(i<sentToCount && i<receivedFromCount)
            fprintf(stdout,"%30s %4i |%30s %4i\n", sentTo[i], sentToMailsCount[i], receivedFrom[i], receivedFromMailsCount[i]);
        else if (i<sentToCount && i>=receivedFromCount)
            fprintf(stdout,"%67s %4i\n", sentTo[i], sentToMailsCount[i]);
        else if (i>=sentToCount && i<receivedFromCount)
            fprintf(stdout,"%30s %4i\n", receivedFrom[i], receivedFromMailsCount[i]);
    }
    //----------------------------------------------------------------
    // Освобождение выделенных ресурсов
	free_resources_receivedFrom:
    for (int i=0; i<receivedFromCount; i++){
        free(receivedFrom[i]);
    }
    free(receivedFrom);
    free(receivedFromMailsCount);

    free_resources_sentTo:
    for (int i=0; i<sentToCount; i++) {
        free(sentTo[i]);
    }
    free(sentTo);
    free(sentToMailsCount);
    sqlite3_finalize(res);
}