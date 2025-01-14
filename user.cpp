#define SQL_NOUNICODEMAP

#include <iostream>
#include "sql.h"
#include "sqlext.h"
#include <vector>
#include <string> 
#include "Crow/include/crow.h"
#include "argon2.h"
#include <cstring>
#include <random>
#include <string>

#pragma comment(lib, "bcrypt.lib")
//user table
    // id SERIAL PRIMARY KEY,
    // username VARCHAR(30) NOT NULL UNIQUE,
    // email VARCHAR(50) NOT NULL UNIQUE,
    // user_pass VARCHAR(256) NOT NULL,
    // user_role VARCHAR(20) DEFAULT 'General',
    // created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    // updated_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    // phone_number VARCHAR(15) NOT NULL UNIQUE

class User {
    private:
        std::string username;
        std::string email;
        std::string password;
        std::string role;
        std::string phone_number;
        int id;
    
    public:
        User(std::string username, std::string email, std::string password, std::string role, std::string pnum, int id){
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
        int getId() {return id;}
    
        //setters 
        void setUsername(std::string new_username) {username = new_username;}
        void setEmail(std::string new_Email) {email = new_Email;}
        void setPasswrod(std::string new_Password) {password = new_Password;}
        void setRole(std::string new_Role) {role = new_Role;}
        void setPhoneNumber(std::string new_phone_number) {phone_number = new_phone_number;}
        void setId(int new_id){ id = new_id;}
};

//just to see if anything db related is successful
void checkReturnCode(SQLRETURN retcode, const std::string& message){
    if(retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Error: " << message<< std::endl;
        exit(EXIT_FAILURE);
    }
}

//connect to db
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

//disconnect from db
void disconnectDatabase(SQLHENV hEnv, SQLHDBC hDbc) {
    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

//print query
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

//returns string
std::string executeQueryReturnString(SQLHDBC hDbc, const std::string& query) {
    SQLHSTMT stmt = NULL;

    SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &stmt);
    checkReturnCode(retcode, "SQL statement handle");

    retcode = SQLExecDirect(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    checkReturnCode(retcode, "SQL query");

    SQLCHAR colData[256];
    SQLLEN colDataLen;

    std::string queryString;

    while(SQLFetch(stmt) == SQL_SUCCESS){
        retcode = SQLGetData(stmt, 1 , SQL_C_CHAR, colData, sizeof(colData), &colDataLen);
        checkReturnCode(retcode, "Column Data");

        std::string row(reinterpret_cast<char*> (colData), colDataLen);

        if(!queryString.empty()) {
            queryString += "\n";
        }
        queryString += row;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return queryString;

}

//verification for registration info
std::string registration(SQLHDBC hDbc, std::string username, std::string password, std::string email, std::string phone_number){
    std::string query;

    //username - verify if there is no other username like that
    query = "SELECT * FROM users WHERE username = " + username + ";";
    std::string result = executeQueryReturnString(hDbc, query);

    if(result.length() != 0){
        return "Username Taken";
    }

    //email - no other accs using same email
    query = "SELECT * FROM users WHERE email LIKE " + email + ";";
    result = executeQueryReturnString(hDbc, query);
    if(result.length() != 0){
        return "Email already used for registered account";
    }

    //password - need to encrypt password
    std::string hashed_password = generateEncryption(password, 16);
    //no checks for phone number ? 

    //update table here ? 
    //insertion query
    query = "INSERT INTO users (username, user_pass, email, phone_number) VALUES(\"" + username + "\", \""  + hashed_password + "\", \"" + email + "\", \"" + phone_number + "\")";
    executeQuery(hDbc, query);
    return "Registration Success";
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

//verify password with hash
bool verifyPassword(std::string password, std::string &hash){
    const char *charpass = password.c_str();
    const char *charHash = hash.c_str();

    int result = argon2id_verify(charHash, charpass, strlen(charpass));

    return result == ARGON2_OK;
}

//for encryption
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

//  //creates new user in user table
// void insertIntoUserDB(SQLHDBC hDbc, std::vector<std::string> inputs){
//     std::string query = "INSERT INTO users (username, user_pass, email, phone_number) VALUES(";
//     query += "'" + inputs.at(0) + "', "
//             + "'" + inputs.at(1) + "', "
//             + "'" + inputs.at(2) + "', "
//             + "'" + inputs.at(3) + "'";
//     executeQuery(hDbc, query);
// }


//-1 == no username exists
//-2 == password is not correct
//0 == password verified 
int verifyLogin(SQLHDBC hDbc, std::string username, std::string password){
    std::string query = "SELECT u.passwords FROM user AS u WHERE u.username LIKE " + username + ";";
    std::string hashedpassword = executeQueryReturnString(hDbc, query);
    if(hashedpassword.length() == 0) {
        return -1;
    }
    if(verifyPassword(password, hashedpassword) == false) {
        return -2;
    } else {
        return 0;
    }
}

