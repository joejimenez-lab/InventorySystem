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
#include <cstdlib>
#include <random>


SQLHENV hEnv;
SQLHDBC hDbc;
SQLRETURN ret;

std::unordered_map<std::string, std::string> sessions;

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

//session token generation
std::string generateSessionToken() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string token;
    for(int i = 0; i < 16; i++){
        token += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return token;
}

int main() {
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
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

    //for starting up immediately
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    crow::SimpleApp app;

    crow::mustache::set_global_base("../html_samples");
    // crow::mustache::set_base("../html_pages");

    std::string login_username;
    std::string login_password;


    //default login page
    CROW_ROUTE(app, "/")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("welcome.html").render(ctx));
    });

    //welcome page
    CROW_ROUTE(app, "/welcome.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("welcome.html").render(ctx));
    });

    // //login page
    CROW_ROUTE(app, "/login.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("login.html").render(ctx));
    });

    // //login handler
    CROW_ROUTE(app, "/login").methods("POST"_method)([&login_username, &login_password](const crow::request& req) {
        auto form_data = parse_urlencoded(req.body);

        login_username = form_data["username"];
        login_password = form_data["password"];



        crow::mustache::context ctx;

        if(login_username.empty() || login_password.empty()) {
            
            ctx["error"] = "Username and Password cannot be empty.";
            return crow::response(crow::mustache::load("login.html").render(ctx));
        }
        
        std::string stored_pass = getPassword(hDbc, login_username);
        int login_val = verifyLogin(hDbc, login_username, login_password);
        std::cout << getPassword(hDbc, login_username) << std::endl;
        if((stored_pass != login_password && login_val == -1) || (stored_pass != login_password && login_val == -2)) {
            ctx["error"] = "Invalid Username or Password";
            return crow::response(crow::mustache::load("login.html").render(ctx));
        } 

        
        //wip
        std::string token = generateSessionToken();
        sessions[token] = login_username;
        

        std::string role = getRole(hDbc, login_username);
        std::cout << role << std::endl;


        crow::response res_redirect;
        res_redirect.code = 302;
        if(role == "General"){
            res_redirect.set_header("Location", "/library_homepage.html");
        } 

        if(role == "Librarian"){
            res_redirect.set_header("Location", "/admin_dashboard.html");
        }
        
        return res_redirect;
         
    });

    //needs fixing
    CROW_ROUTE(app, "/sign-up.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("sign-up.html").render(ctx));        
    });
    
    //needs fixing
    CROW_ROUTE(app, "/registration").methods("POST"_method)([](const crow::request& req) {
        auto form_data = parse_urlencoded(req.body);
        std::vector<std::string> registration_info;
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

    CROW_ROUTE(app, "/user_restrictions.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_restrictions.html").render(ctx));
    });

    CROW_ROUTE(app, "/user_profile.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_profile.html").render(ctx));
    });

    CROW_ROUTE(app, "/user_management.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_management.html").render(ctx));
    });

    CROW_ROUTE(app, "/user_library_interaction.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_library_interaction.html").render(ctx));
    });

    CROW_ROUTE(app, "/user_book_reviews.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_book_reviews.html").render(ctx));
    });

    CROW_ROUTE(app, "/stock_update.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("stock_update.html").render(ctx));
    });

    CROW_ROUTE(app, "/search_results.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("search_results.html").render(ctx));
    });

    CROW_ROUTE(app, "/return_books.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("return_books.html").render(ctx));
    });

    CROW_ROUTE(app, "/reset_password.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("reset_password.html").render(ctx));
    });

    CROW_ROUTE(app, "/recommendations.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("recommendations.html").render(ctx));
    });

    CROW_ROUTE(app, "/reading_challenge.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("reading_challenge.html").render(ctx));
    });

    CROW_ROUTE(app, "/notifications_dashboard.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("notifications_dashboard.html").render(ctx));
    });
    
    CROW_ROUTE(app, "/my_borrowed_books.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("my_borrowed_books.html").render(ctx));
    });
    
    CROW_ROUTE(app, "/library_database.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_database.html").render(ctx));
    });

    CROW_ROUTE(app, "/library_database_management.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_database_management.html").render(ctx));
    });

    CROW_ROUTE(app, "/librarian_stock_alert.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("librarian_stock_alert.html").render(ctx));
    });

    CROW_ROUTE(app, "/group_forum_homepage.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("group_forum_homepage.html").render(ctx));
    });

    CROW_ROUTE(app, "/edit_user_role.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("edit_user_role.html").render(ctx));
    });

    CROW_ROUTE(app, "/edit_book_information.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("edit_book_information.html").render(ctx));
    });

    CROW_ROUTE(app, "/due_date_reminder.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("due_date_reminder.html").render(ctx));
    });

    CROW_ROUTE(app, "/discussion_forum.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("discussion_forum.html").render(ctx));
    });

    CROW_ROUTE(app, "/device_setup.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("device_setup.html").render(ctx));
    });

    CROW_ROUTE(app, "/delete_book.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("delete_book.html").render(ctx));
    });

    CROW_ROUTE(app, "/data_visualization_dashboard.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("data_visualization_dashboard.html").render(ctx));
    });

    CROW_ROUTE(app, "/check_out.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("check_out.html").render(ctx));
    });

    CROW_ROUTE(app, "/check_in.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("check_in.html").render(ctx));
    });

    CROW_ROUTE(app, "/chatbot.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("chatbot.html").render(ctx));
    });

    CROW_ROUTE(app, "/book_reservation.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("book_reservation.html").render(ctx));
    });

    CROW_ROUTE(app, "/book_details.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("book_details.html").render(ctx));
    });   

    CROW_ROUTE(app, "/book_available_alert.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("book_available_alert.html").render(ctx));
    });

    CROW_ROUTE(app, "/barcode_scanner.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("barcode_scanner.html").render(ctx));
    });
    
    CROW_ROUTE(app, "/add_new_book.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("add_new_book.html").render(ctx));
    });

    CROW_ROUTE(app, "/activity_logs.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("activity_logs.html").render(ctx));
    });

    CROW_ROUTE(app, "/account_security_settings")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("account_security_settings").render(ctx));
    });

    CROW_ROUTE(app, "/forgot_password.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("forgot_password.html").render(ctx));
    });


    //homepages
    CROW_ROUTE(app, "/library_homepage.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_homepage.html").render(ctx));
    });

    CROW_ROUTE(app, "/admin_dashboard.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("admin_dashboard.html").render(ctx));
    });


    //Error pages
    CROW_ROUTE(app, "/403.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("error_pages/403.html").render(ctx));
    });

    CROW_ROUTE(app, "/404.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("error_pages/404.html").render(ctx));
    });

    CROW_ROUTE(app, "/500.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("error_pages/500.html").render(ctx));
    });

    CROW_ROUTE(app, "/503.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("error_pages/503.html").render(ctx));
    });
    std::thread serverThread([&app]() {
        app.port(8080).multithreaded().run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

#ifdef _WIN32
    std::system("start http://localhost:8080");
#elif __APPLE__
    std::system("open http://localhost:8080");
#elif __linux__
    std::system("xdg-open http://localhost:8080");
#endif


    serverThread.join();

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