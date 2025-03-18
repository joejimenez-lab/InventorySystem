#ifndef USER_H
#define USER_H

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


class User {
    private:
        std::string username;
        std::string email;
        std::string password;
        std::string role;
        std::string phone_number;
        std::string id;
    
    public:
        User() : username(""), email(""), password(""), role(""), phone_number(""), id("") {}

        User(std::string username, std::string email, std::string password, std::string role, std::string pnum, std::string id){
            this->username = username;
            this->email = email;
            this->password = password;
            this->role = role;
            this->phone_number = pnum;
            this->id = id;
        }

        //getters
        std::string getUsername() {return username;}
        std::string getEmail() {return email;}
        std::string getPassword() {return password;}
        std::string getRole() {return role;}
        std::string getPhoneNumber() {return phone_number;}
        std::string getId() {return id;}
    
        //setters 
        void setUsername(std::string new_username) {username = new_username;}
        void setEmail(std::string new_Email) {email = new_Email;}
        void setPasswrod(std::string new_Password) {password = new_Password;}
        void setRole(std::string new_Role) {role = new_Role;}
        void setPhoneNumber(std::string new_phone_number) {phone_number = new_phone_number;}
        void setId(std::string new_id){ id = new_id;}
};

void checkReturnCode(SQLRETURN retcode, const std::string& message);

SQLHDBC connectToDB(SQLHENV& hEnv, const std::string& connectionString);

void disconnectDatabase(SQLHENV hEnv, SQLHDBC hDbc);

void executeQuery(SQLHDBC hDbc, const std::string& query);

std::string executeQueryReturnString(SQLHDBC hDbc, const std::string& query);

std::string registration(SQLHDBC hDbc, std::string username, std::string password, std::string email, std::string phone_number);

std::string generateEncryption(std::string password, int saltLength);

bool verifyPassword(std::string password, std::string& hash);

std::string generateRandomSalt(int length);

void insertIntoUserDB(SQLHDBC hDbc, std::vector<std::string> inputs);

int verifyLogin(SQLHDBC hDbc, std::string username, std::string password);

std::string getRole(SQLHDBC, std::string username);

std::string getPassword(SQLHDBC hDbc, std::string username);

bool changePassword(SQLHDBC hDbc, std::string username, std::string currpass, std::string newpass);

std::vector<std::vector<std::string>> executeQueryReturnRows(SQLHDBC hDbc, std::string& query);

std::string execute_python_script(const std::string& url);

#endif