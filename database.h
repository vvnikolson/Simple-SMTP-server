//
// Created by vvnikolson on 4/28/19.
//

#ifndef SOC_DATABASE_H
#define SOC_DATABASE_H

#include "includings.h"

void create_tables(sqlite3 *pDB);

sqlite3* initialize_database();

int add_user(sqlite3 *, const char*);

int get_user_id(sqlite3*, const char*);

int save_mail(sqlite3*, const char*, int, int*, int);

void print_user_info(sqlite3 *pDB, int id);


#endif //SOC_DATABASE_H
