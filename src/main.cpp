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
#include "books.h"
#include "admin.h"
#include <cpr/cpr.h>
#include "json/json.h"


SQLHENV hEnv;
SQLHDBC hDbc;
SQLRETURN ret;

std::unordered_map<std::string, std::string> sessions;
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> user_data;

void signalHandler(int signal);

std::string load_html(const std::string& file_path) {
    std::ifstream html_file(file_path);
    std::stringstream buffer;
    buffer << html_file.rdbuf();
    return buffer.str();
}

std::string html_escape(const std::string& input) {
    std::ostringstream escaped;
    for (char c : input) {
        switch (c) {
            case '&':  escaped << "&amp;"; break;
            case '<':  escaped << "&lt;"; break;
            case '>':  escaped << "&gt;"; break;
            case '"':  escaped << "&quot;"; break;
            case '\'': escaped << "&#39;"; break;
            default:   escaped << c; break;
        }
    }
    return escaped.str();
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
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<int> dist(0, 15);
    static const char* hex_chars = "0123456789abcdef";

    std::string session_id;
    for (int i = 0; i < 32; ++i)
        session_id += hex_chars[dist(rng)];
    return session_id;
}

std::string extractSessionID(const std::string& cookie) {
    size_t pos = cookie.find("session_id=");
    if (pos != std::string::npos) {
        size_t end = cookie.find(";", pos);
        return cookie.substr(pos + 11, end - pos - 11); 
    }
    return "";
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
        if((stored_pass != login_password && login_val == -1) || (stored_pass != login_password && login_val == -2)) {
            ctx["error"] = "Invalid Username or Password";
            return crow::response(crow::mustache::load("login.html").render(ctx));
        } 

        
        //wip
        std::string token = generateSessionToken();
        sessions[token] = login_username;
        user_data[token]["username"] = login_username;

        std::string role = getRole(hDbc, login_username);

        crow::response res_redirect;
        res_redirect.code = 302;

        if(role == "General"){
            res_redirect.set_header("Location", "/library_homepage.html");
        } 

        if(role == "Librarian"){
            res_redirect.set_header("Location", "/admin_dashboard.html");
        }
        
        res_redirect.set_header("Set-Cookie", "session_id=" + token + "; Path=/; HttpOnly");
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
    
        for (const auto& info : registration_info) {
            std::cout << info << std::endl;
        }
    
        std::cout << "Adding into Database" << std::endl;
    
        // Check registration result
        if (registration(hDbc, registration_info[0], registration_info[1], registration_info[2], registration_info[3]) != "Registration Success") {
            crow::mustache::context ctx;
            ctx["error"] = "Invalid Username or Email Taken";
            ctx["username"] = registration_info[0];  // Preserve username
            ctx["email"] = registration_info[2];     // Preserve email
            ctx["phone"] = registration_info[3];     // Preserve phone
    
            return crow::response(crow::mustache::load("sign-up.html").render(ctx));
        }
    
        // Redirect to login page if successful
        crow::response res;
        res.set_header("Location", "/login.html");
        res.code = 303;
        return res;
    });
    

    CROW_ROUTE(app, "/user_restrictions.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_restrictions.html").render(ctx));
    });

    CROW_ROUTE(app, "/get_restrictions").methods("GET"_method)
    ([](const crow::request& req) {
        auto params = crow::query_string{req.url_params};
        std::string user_id_str = params.get("user_id") ? params.get("user_id") : "";

        if (user_id_str.empty()) {
            return crow::response(400, "Missing user_id");
        }

        std::string query = "SELECT borrow, reservation, resource FROM users_restrictions WHERE user_id = " + user_id_str + ";";
        std::vector<std::vector<std::string>> info = executeQueryReturnRows(hDbc, query);
        if (info.empty()) {
            return crow::response(404, "User not found");
        }

        crow::json::wvalue json_response;
        json_response["borrow"] = (info[0][0] == "1");  // Convert "1"/"0" to boolean
        json_response["reserve"] = (info[0][1] == "1");
        json_response["access"] = (info[0][2] == "1");

        return crow::response(json_response.dump());
    });

    CROW_ROUTE(app, "/update_restrictions").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Invalid JSON");
        }
    
        std::string user_id = body["user_id"].s();
        std::string borrow = body["borrow"].b() ? "true" : "false";  
        std::string reserve = body["reserve"].b() ? "true" : "false";
        std::string access = body["access"].b() ? "true" : "false";
    
        if (user_id.empty()) {
            return crow::response(400, "Missing user_id");
        }
    
        std::string query = "UPDATE users_restrictions SET borrow = " + borrow + ", reservation = " + reserve + ", resource = " + access + " WHERE user_id = " + user_id + ";";
    
        std::string success = executeQueryReturnString(hDbc, query);
    
        
        return crow::response(200, "{\"status\":\"success\", \"message\":\"Restrictions updated successfully\"}");
       
    });

    CROW_ROUTE(app, "/user_profile.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("user_profile.html").render(ctx));
    });
    
    CROW_ROUTE(app, "/api/users")
    .methods("GET"_method)([]() {
        crow::json::wvalue result;
        std::string query = "SELECT id, username, user_role FROM users ORDER BY id;";
        std::vector<std::vector<std::string>> allusers = executeQueryReturnRows(hDbc, query);
        std::vector<crow::json::wvalue> user_list;
        for (const auto& user : allusers) {
            if (user.size() >= 3) {
                user_list.push_back(crow::json::wvalue{
                    {"id", user[0]},
                    {"username", user[1]},
                    {"role", user[2]}
                });
            }
        }
        result["users"] = std::move(user_list);
        
        return crow::response{result};
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

    CROW_ROUTE(app, "/genre_preferences.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("genre_preferences.html").render(ctx));
    });


    //homepages
    CROW_ROUTE(app, "/library_homepage.html")([](const crow::request& req){
        std::string session_cookie = req.get_header_value("Cookie");
        std::string token = extractSessionID(session_cookie);

        if(sessions.find(token) == sessions.end()) {
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/403.html");
            return res;
        }

        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_homepage.html").render(ctx));
    });

    CROW_ROUTE(app, "/search_results")([](const crow::request& req) {
        auto query = req.url_params.get("query"); // Get search query

        if (!query) {
            return crow::response(400, "No query provided.");
        }
        
        std::string search_result = html_escape(query);
        std::ostringstream response_html;

        // testing
        // std::cout << search_result << std::endl;
        // std::cout << query << std::endl;
        
        return crow::response{response_html.str()};
    });

    CROW_ROUTE(app, "/admin_dashboard.html")([](const crow::request& req){
        std::string session_cookie = req.get_header_value("Cookie");
        std::string token = extractSessionID(session_cookie);

        if(sessions.find(token) == sessions.end()) {
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/403.html");
            return res;
        }

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