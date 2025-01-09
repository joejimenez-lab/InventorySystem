#include <iostream>
#include "sql.h"
#include "sqlext.h"
#include <vector>
#include <string> 
#include "Crow/include/crow.h"
#include "argon2.h"
#include <cstring>
#include <random>

#pragma comment(lib, "bcrypt.lib")




void checkReturnCode(SQLRETURN retcode, const std::string& message){
    if(retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Error: " << message<< std::endl;
        exit(EXIT_FAILURE);
    }
}

SQLHDBC connectToDB(SQLHENV& hEnv, const std::string& connectionString) {
    SQLHDBC hDbc = NULL;
    
    SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv); 
    checkReturnCode(retcode, "Environment Handle");

    retcode = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    checkReturnCode(retcode, "ODBC version");

    retcode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    checkReturnCode(retcode, "Connection Handle");

    retcode = SQLDriverConnect(hDbc, NULL, (SQLCHAR*)connectionString.c_str(), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE);
    checkReturnCode(retcode, "Connection to Database");

    std::cout << "Connected to Database" << std::endl;
    return hDbc;
}

void disconnectDatabase(SQLHENV hEnv, SQLHDBC hDbc) {
    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

void executeQuery(SQLHDBC hDbc, const std::string& query) {
    SQLHSTMT stmt = NULL;

    SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    checkReturnCode(retcode, "Executing SQL query");

    SQLCHAR colData[256];
    while(SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, colData, sizeof(colData), NULL);
        std::cout << "Data: " << colData << std::endl;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

//placeholder for getting info from frontend
std::vector<std::string> getInfoRegistration(){
    std::vector<std::string> inputs;
    std::string curr_input;
    //username
    std::cout << "Enter Username" << std::endl;
    std::cin >> curr_input;
    inputs.push_back(curr_input);

    //password
    std::cout << "Enter Password" << std::endl;
    std::cin >> curr_input;
    inputs.push_back(curr_input);

    //email
    std::cout << "Enter Email" << std::endl;
    std::cin >> curr_input;
    inputs.push_back(curr_input);

    std::cout << "Enter Phone Number" << std::endl;
    std::cin >> curr_input;
    inputs.push_back(curr_input);

    return inputs;
}

//password encryption
std::string generateEncryption(std::string password, int saltLength){
    const char* charpass = password.c_str();
    const char* salt = generateRandomSalt(saltLength).c_str();

    char hash[32];
    char encoded[128];
    int hash_len = 32;
    int encoded_len = 128;

    int t_cost = 2;
    int m_cost =  1 << 16;
    int parallelism = 1;
    int result;

    result = argon2id_hash_encoded(t_cost, m_cost, parallelism, charpass, strlen(charpass), salt, strlen(salt), hash_len, (char *)encoded, encoded_len);

    return std::string(encoded);
}

bool verifyPassword(std::string password, std::string &hash){
    const char *charpass = password.c_str();
    const char *charHash = hash.c_str();

    int result = argon2id_verify(charHash, charpass, strlen(charpass));

    return result == ARGON2_OK;
}

std::string generateRandomSalt(int length) {
    const std::string charSet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    std::random_device seed;
    std::mt19937 gen{seed()};
    std::uniform_int_distribution<> dist{0, charSet.size() - 1};

    std::string salt;

    for(int i = 0; i < length; i++){
        salt += charSet[dist(gen)];
    }

    return salt;
}

//creates new user in table
void insertIntoUserDB(SQLHDBC hDbc, std::vector<std::string> inputs){
    std::string query = "INSERT INTO users (username, user_pass, email, phone_number) VALUES(";
    query += "'" + inputs.at(0) + "', "
            + "'" + inputs.at(1) + "', "
            + "'" + inputs.at(2) + "', "
            + "'" + inputs.at(3) + "'";
    executeQuery(hDbc, query);
}