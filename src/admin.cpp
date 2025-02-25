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
#include "admin.h"
#include "user.h"

void changeRole(SQLHDBC hDbc, std::string user){
    std::string query = "UPDATE users SET user_role = Admin WHERE username = '" + user + "';";
    executeQuery(hDbc, query);
}

void removeReservation(SQLHDBC hDbc, int id){
    std::string id_string = std::to_string(id);
    std::string query = "DELETE FROM reserved_books WHERE reserve_id = " + id_string + ";";
    executeQuery(hDbc, query);
}