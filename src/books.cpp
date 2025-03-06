#define SQL_NOUNICODEMAP

#include "user.h"
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
#include "argon2.h"
#include <cstring>
#include <random>
#include <string>
#include "user.h"
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

void insertBook(SQLHDBC hDbc, std::string book_author, std::string book_title, std::string genre, int publication_year, int isbn, int total_copies){
    std::string query = "";

    std::ostringstream oss;
    oss << isbn;
    std::string isbnStr = oss.str();

    //check if book exists
    query = "SELECT copies_available FROM books WHERE isbn = " + isbnStr + ";";
    std::string numCopies = executeQueryReturnString(hDbc, query);

    if(!numCopies.empty()){
        std::cout << "Book with ISBN " << isbn << " already exists." << std::endl;
        return
    }

    std::ostringstream oss2;
    oss2 << "INSERT INTO books (title, author, genre, publication_year, isbn, copies_available) VALUES ('"
        << book_title << "', '" 
        << book_author << "', '" 
        << genre << "', " 
        << publication_year << ", " 
        << isbn << ", " 
        << total_copies << ");";

    query = oss.str();
    executeQuery(hDbc, query);
}


