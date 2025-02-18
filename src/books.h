#ifndef BOOKS_H
#define BOOKS_H

#include <windows.h>  
#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <minwindef.h> 
#include <sysinfoapi.h>
#include <windef.h>
#include <wtypesbase.h>
#include <iostream>
#include <sql.h>
#include <vector>
#include <string> 
#include <cstring>
#include <random>
#include <string>
#include "user.h"

void insertBook(SQLHDBC hDbc, std::string book_name, std::string book_title, std::string genre, int publication_year, int isbn );


#endif