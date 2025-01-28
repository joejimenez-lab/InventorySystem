#include "crow.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <string>
#include <crow/query_string.h>
#include <iomanip>
#include <map>
#include "user.h"
#include "sql.h"
#include "sqlext.h"
#include "argon2.h"
#include <signal.h>

SQLHENV hEnv;
SQLHDBC hDbc;
SQLRETURN ret;


void signalHandler(int signal);

std::string load_html(const std::string& file_path) {
    std::ifstream html_file(file_path);
    std::stringstream buffer;
    buffer << html_file.rdbuf();
    return buffer.str();
}

std::unordered_map<std::string, std::string> parse_form_data(const std::string& body) {
    std::unordered_map<std::string, std::string> form_data;
    std::istringstream stream(body);
    std::string key_value_pair;
    
    while (std::getline(stream, key_value_pair, '&')) {
        size_t pos = key_value_pair.find('=');
        if (pos != std::string::npos) {
            std::string key = key_value_pair.substr(0, pos);
            std::string value = key_value_pair.substr(pos + 1);
            form_data[key] = value;
        }
    }
    
    return form_data;
}

//Need to create url decoder for @ and stuff
std::string url_decode(const std::string& value) {
    std::ostringstream decoded;
    for(size_t i = 0; i < value.length(); i++){
        if(value[i] == '%') {
            if(i + 2 < value.length()) {
                std::istringstream hex(value.substr(i + 1, 2));
                int character;
                hex >> std::hex >> character;
                decoded << static_cast<char>(character);
                i += 2;
            }
        } else if(value[i] == '+') {
            decoded << ' ';
        } else {
            decoded << value[i];
        }
    }
    return decoded.str();
}

std::map<std::string, std::string> parse_urlencoded(const std::string& body) {
    std::map<std::string, std::string> result;
    size_t pos = 0, found;

    while ((found = body.find('&', pos)) != std::string::npos) {
        auto pair = body.substr(pos, found - pos);
        auto eq_pos = pair.find('=');

        if (eq_pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, eq_pos));
            std::string value = url_decode(pair.substr(eq_pos + 1));

            
            result[key] = value;
        }

        pos = found + 1;
    }


    auto pair = body.substr(pos);
    auto eq_pos = pair.find('=');

    if (eq_pos != std::string::npos) {
        std::string key = url_decode(pair.substr(0, eq_pos));
        std::string value = url_decode(pair.substr(eq_pos + 1));

        result[key] = value;
    }

    return result;
}


int main() {
    SQLCHAR* datasource = (SQLCHAR*)"PostgreSQL30";  // DSN name
    SQLCHAR* user = (SQLCHAR*)"postgres";  // Database username
    SQLCHAR* pass = (SQLCHAR*)"Project";  // Database password
    
    //connect to db


    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to allocate environment handle." << std::endl;
        return 1;
    }

    // Set the ODBC version environment attribute
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to set ODBC version." << std::endl;
        return 1;
    }

    // Allocate connection handle
    ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to allocate connection handle." << std::endl;
        return 1;
    }


    ret = SQLConnect(hDbc, (SQLCHAR*)datasource, SQL_NTS, 
                     (SQLCHAR*)user, SQL_NTS, 
                     (SQLCHAR*)pass, SQL_NTS);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        std::cout << "Connection successful!" << std::endl;
    } else {
        std::cerr << "Connection failed!" << std::endl;
    }
    SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    crow::SimpleApp app;

    std::string login_username;
    std::string login_password;
    std::vector<std::string> registration_info;

    CROW_ROUTE(app, "/")([](){
        std::string html_content = load_html("../html_pages/login.html");
        return html_content;
    });

    CROW_ROUTE(app, "/login").methods("POST"_method)([&login_username, &login_password](const crow::request& req) {
        auto form_data = parse_urlencoded(req.body);

        
        login_username = form_data["username"];
        login_password = form_data["password"];

        std::cout << verifyLogin(hDbc, login_username, login_password) << std::endl;
        //do something with verify login
        return crow::response(200, "Form submitted successfully");
    });

    CROW_ROUTE(app, "/registration.html")([](){
        std::string html_content = load_html("../html_pages/registration.html");
        return html_content;
    });
    
    CROW_ROUTE(app, "/registration").methods("POST"_method)([&registration_info](const crow::request& req) {
        auto form_data = parse_urlencoded(req.body);

        registration_info.push_back(form_data["username"]);
        registration_info.push_back(form_data["password"]);
        registration_info.push_back(form_data["email"]);
        registration_info.push_back(form_data["phone"]);

        for(int i = 0; i < registration_info.size(); i++){
            std::cout << registration_info.at(i) << std::endl;
        }
        std::cout << "Adding into Database" << std::endl;
        std::cout << registration(hDbc, registration_info.at(0), registration_info.at(1), registration_info.at(2), registration_info.at(3)) << std::endl;

        return crow::response(200, "Form submitted successfully");

    });

    app.port(8080).multithreaded().run();

    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    std::cout << "Database disconnected and resources freed." << std::endl;

    return 0;
}


void signalHandler(int signal){
    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    exit(signal);
}