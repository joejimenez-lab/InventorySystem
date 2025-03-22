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
#include <cctype>


SQLHENV hEnv;
SQLHDBC hDbc;
SQLRETURN ret;

std::unordered_map<std::string, std::string> sessions;
std::unordered_map<std::string, User> user_data;

void signalHandler(int signal);

std::string load_html(const std::string& file_path) {
    std::ifstream html_file(file_path);
    std::stringstream buffer;
    buffer << html_file.rdbuf();
    return buffer.str();
}

std::string get_content_type(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".html", "text/html"}
    };

    size_t dot_pos = path.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string ext = path.substr(dot_pos);
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }
    }
    return "application/octet-stream"; 
}


std::string url_encode(const std::string &str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char ch : str) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else {
        
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
        }
    }

    return encoded.str();
}

std::string escapeApostrophes(const std::string& input) {
    std::string escaped = "";
    for (char c : input) {
        if (c == '\'') {
            escaped += "''";  
        } else {
            escaped += c;
        }
    }
    return escaped;
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

    SQLCHAR* datasource = (SQLCHAR*);  // DSN name
    SQLCHAR* user = (SQLCHAR*);  // Database password
    
    //connect to db
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to allocate environment handle." << std::endl;
        return 1;
    }

    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to set ODBC version." << std::endl;
        return 1;
    }

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
        std::string query = "SELECT * FROM users WHERE username = '" + login_username + "';";
        std::vector<std::vector<std::string>> userinfo = executeQueryReturnRows(hDbc, query);
        User user(userinfo[0][1], userinfo[0][2], login_password, userinfo[0][4], userinfo[0][7], userinfo[0][0]);
        user_data[token] = user;
        
        std::string role = user.getRole();
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
 
        if (registration(hDbc, registration_info[0], registration_info[1], registration_info[2], registration_info[3]) != "Registration Success") {
            crow::mustache::context ctx;
            ctx["error"] = "Invalid Username or Email Taken";
            ctx["username"] = registration_info[0];  
            ctx["email"] = registration_info[2];     
            ctx["phone"] = registration_info[3];    
    
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
        json_response["borrow"] = (info[0][0] == "1");  
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

    CROW_ROUTE(app, "/randomRecs")
    .methods("GET"_method)([](const crow::request& req) {
        

        std::string token;
        auto cookies = req.headers.find("Cookie");
        if (cookies != req.headers.end()) {
            std::string cookie_header = cookies->second;
            size_t token_start = cookie_header.find("session_id=");
            if (token_start != std::string::npos) {
                token_start += 11; 
                size_t token_end = cookie_header.find(";", token_start);
                token = cookie_header.substr(token_start, token_end - token_start);
            }
        }

        User user = user_data[token];
        std::string user_id = user.getId();

        
        std::string query = "SELECT b.book_id, b.title, b.author, b.genre FROM books b JOIN userGenres ug ON ug.user_id = " + user_id + " WHERE ',' || ug.genres || ',' LIKE '%,' || b.genre || ',%' oRDER BY RANDOM() LIMIT 3;";
        std::vector<std::vector<std::string>> rows = executeQueryReturnRows(hDbc, query);
        std::cout << rows[0][0] << std::endl;
        std::cout << user_id << std::endl;
    
        crow::json::wvalue response;
        crow::json::wvalue::list books;

        for (const auto& row : rows) {
            crow::json::wvalue book;
            book["title"] = row[1];   
            book["author"] = row[2];  
            book["genre"] = row[3];   
            book["book_id"] = row[0];
            books.push_back(book);
        }

        response["books"] = std::move(books);
        return crow::response{response};

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

    CROW_ROUTE(app, "/getBorrowedBooks")
    ([](const crow::request& req) {
        
        std::string token;
        auto cookies = req.headers.find("Cookie");
        if (cookies != req.headers.end()) {
            std::string cookie_header = cookies->second;
            size_t token_start = cookie_header.find("session_id=");
            if (token_start != std::string::npos) {
                token_start += 11; 
                size_t token_end = cookie_header.find(";", token_start);
                token = cookie_header.substr(token_start, token_end - token_start);
            }
        }

        User user = user_data[token];
        std::cout << user.getId() << std::endl;
        std::string query = "SELECT books.title, bb.borrow_date, bb.return_date FROM borrowed_books bb JOIN books ON bb.book_id = books.book_id WHERE bb.user_id = " + user.getId() + ";";
        std::vector<std::vector<std::string>> borrowedBooks = executeQueryReturnRows(hDbc, query);


        crow::json::wvalue response;
        crow::json::wvalue::list books;

        for (const auto& row : borrowedBooks) {
            crow::json::wvalue book;
            book["title"] = row[0];
            book["borrow_date"] = row[1];
            book["due_date"] = row[2];
            books.push_back(book);
        }

        response["books"] = std::move(books);
        return crow::response{response};
    });
    
    CROW_ROUTE(app, "/library_database.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_database.html").render(ctx));
    });

    CROW_ROUTE(app, "/library_database_management.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("library_database_management.html").render(ctx));
    });

    
    CROW_ROUTE(app, "/api/books")
    .methods("GET"_method)([](const crow::request& req) {
        crow::json::wvalue result;
        int limit = 20;
        int page = 1;
        std::string searchQuery = "";
        if (req.url_params.get("page")) {
            page = std::stoi(req.url_params.get("page"));
            if (page < 1) page = 1;
        }

        if (req.url_params.get("search")) {
            searchQuery = req.url_params.get("search");
        }

        int offset = (page - 1) * limit;
        std::string query = "";
        std::vector<std::vector<std::string>> allbooks;
        if(searchQuery == ""){
            query = "SELECT book_id, title, author, copies_available FROM books ORDER BY book_id LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset) + ";";
            allbooks = executeQueryReturnRows(hDbc, query);
        } else {
            query = "SELECT book_id, title, author, copies_available FROM books WHERE book_id = " + searchQuery + " ORDER BY book_id LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset) + ";";
            allbooks = executeQueryReturnRows(hDbc, query);
        }
        
        
        std::vector<crow::json::wvalue> book_list;
        for (const auto& book : allbooks) {
            if (book.size() >= 4) {
                book_list.push_back(crow::json::wvalue{
                    {"id", book[0]},
                    {"title", book[1]},
                    {"author", book[2]},
                    {"availability", book[3]}
                });
            }
        }
        result["books"] = std::move(book_list);
        result["page"] = page;
        return crow::response{result};
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

    CROW_ROUTE(app, "/update_role").methods("POST"_method)([](const crow::request& req) {
        // Parse the form data
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Invalid JSON");
        }
        std::string username = body["username"].s();
        std::string role = body["role"].s();
        
        std::string query = "UPDATE users SET user_role = '" + role + "' WHERE username = '" + username + "';";
        executeQuery(hDbc, query);

        return crow::response(200, "{\"status\":\"success\", \"message\":\"Role updated successfully\"}");
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

    CROW_ROUTE(app, "/getBookTitle")
        ([](const crow::request& req) {
        std::string book_id = req.url_params.get("book_id");

        std::string query = "SELECT title FROM books WHERE book_id = " + book_id + ";";
        std::string title = executeQueryReturnString(hDbc, query);
        
        crow::json::wvalue response;
        response["title"] = title;
        return response;
    });

    CROW_ROUTE(app, "/getThreadId")
    ([](const crow::request& req) {
        std::string book_id = req.url_params.get("book_id");
        std::string query = "SELECT thread_id FROM discussion_threads WHERE book_id = " + book_id + ";";
        std::string thread_id = executeQueryReturnString(hDbc, query);
        std::cout << thread_id << std::endl;
        crow::json::wvalue response;
        response["threadId"] = thread_id;
        return response;
    });

    CROW_ROUTE(app, "/getComments")
    ([](const crow::request& req) {
        std::string thread_id = req.url_params.get("thread_id");
        int page = req.url_params.get("page") ? std::stoi(req.url_params.get("page")) : 1;
        int comments_per_page = 10;
        int offset = (page - 1) * comments_per_page;

        std::string query = "SELECT comment FROM comments WHERE thread_id = " + thread_id + 
                            " ORDER BY created_at ASC LIMIT " + std::to_string(comments_per_page) + 
                            " OFFSET " + std::to_string(offset) + ";";

        std::vector<std::vector<std::string>> rows = executeQueryReturnRows(hDbc, query);
        std::vector<std::string> comments;
        for(const auto& row : rows) {
            if(!row.empty()){
                comments.push_back(row[0]);
            }
        }
        std::cout << comments[0] << std::endl;
        std::string countQuery = "SELECT COUNT(*) FROM comments WHERE thread_id = " + thread_id + ";";
        std::string total_comments_str = executeQueryReturnString(hDbc, countQuery);
        int total_pages = (std::stoi(total_comments_str) + comments_per_page - 1) / comments_per_page; 

        int total_comments = 0;
        if (!total_comments_str.empty()) {
            total_comments = std::stoi(total_comments_str);
        }

        crow::json::wvalue response;
        response["comments"] = comments;
        response["total_pages"] = total_pages;
        return response;
    });

    CROW_ROUTE(app, "/storeComment").methods("POST"_method)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        int thread_id = json["thread_id"].i();
        std::string comment = json["comment"].s();

        std::string insertSQL = "INSERT INTO comments (thread_id, comment) VALUES (" +
                                std::to_string(thread_id) + ", '" + comment + "');";
        executeQuery(hDbc, insertSQL);


        return crow::response(200, "Comment stored successfully");
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

    CROW_ROUTE(app, "/generateimages")
    .methods("GET"_method)
    ([]
    {
        generateImages();
        return crow::response(200, "Images generated successfully");
    });

    CROW_ROUTE(app, "/assets/<string>")
    ([](const crow::request& req, std::string path) {
        if (path.find("..") != std::string::npos) {
            return crow::response(403, "Forbidden");
        }
    
        std::string file_path = "../assets/" + path;
        std::cout << "Trying to open: " << file_path << std::endl; 
    
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "File not found: " << file_path << std::endl; 
            return crow::response(404, "File not found");
        }
    
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string file_content = buffer.str();
    
        crow::response res(200);
        res.set_header("Content-Type", get_content_type(path));
        res.body = std::move(file_content);
        return res;
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

    CROW_ROUTE(app, "/createReservation/book_id/<string>/date/<string>")
    .methods("POST"_method)([](const crow::request& req, std::string book_id, const std::string& reservation_date){

        std::cout << book_id << std::endl;
        std::cout << reservation_date << std::endl;
        
        std::string token;
            auto cookies = req.headers.find("Cookie");
            if (cookies != req.headers.end()) {
                std::string cookie_header = cookies->second;
                size_t token_start = cookie_header.find("session_id=");
                if (token_start != std::string::npos) {
                    token_start += 11; 
                    size_t token_end = cookie_header.find(";", token_start);
                    token = cookie_header.substr(token_start, token_end - token_start);
                }
            }
        User user = user_data[token];
        std::string user_id = user.getId();
        std::string query = "INSERT INTO reserved_books (book_id, user_id, reserve_date) VALUES (" + book_id + ", " + user_id + ", '" + reservation_date + "');";
        executeQuery(hDbc, query);

    
        return crow::response(200, R"({"success": true})");
        
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

    CROW_ROUTE(app, "/admin_CSV_dragndrop.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("admin_CSV_dragndrop.html").render(ctx));
    });

    CROW_ROUTE(app, "/barcode/<string>")
    .methods("GET"_method)([](const crow::request& req, std::string book_id) {

        std::cout << book_id << std::endl;
        std::string barcode_path = "../barcodes/barcode_" + book_id + ".png";

        if (!std::filesystem::exists(barcode_path)) {
            generateBarcode(book_id);
        }

        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "image/png");

        std::ifstream file(barcode_path, std::ios::binary);
        if (!file) {
            return crow::response(404, "Barcode not found.");
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        res.body = ss.str();

        return res;
    });

    CROW_ROUTE(app, "/genre_preferences.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("genre_preferences.html").render(ctx));
    });



    CROW_ROUTE(app, "/get_genres")
    .methods("GET"_method)([]() {
        crow::json::wvalue genres_json;
        std::string query = "SELECT genre, COUNT(*) FROM books GROUP BY genre;";
        std::vector<std::vector<std::string>> allGenres = executeQueryReturnRows(hDbc, query);
        for(const auto& row : allGenres){
            genres_json[row[0]] = row[1];
        }
        return crow::response{genres_json};
    });

    CROW_ROUTE(app, "/save_genres").methods("POST"_method)
    ([](const crow::request& req) {
            auto json_data = crow::json::load(req.body);

            if (!json_data || !json_data.has("genres")) {
                return crow::response(400, "Invalid JSON format or missing 'genres' field");
            }

            std::vector<std::string> selectedGenres;
            auto genres_array = json_data["genres"]; 

            std::string combinedGenres = "";

            for (int i = 0; i < genres_array.size(); i++) {
                selectedGenres.push_back(genres_array[i].s());
            }

            for (int i = 0; i < selectedGenres.size(); i++) {
                combinedGenres += selectedGenres[i];
                if(i < selectedGenres.size() - 1) {
                    combinedGenres += ", ";
                }
            }
            
            //getting cookies
            std::string token;
            auto cookies = req.headers.find("Cookie");
            if (cookies != req.headers.end()) {
                std::string cookie_header = cookies->second;
                size_t token_start = cookie_header.find("session_id=");
                if (token_start != std::string::npos) {
                    token_start += 11; 
                    size_t token_end = cookie_header.find(";", token_start);
                    token = cookie_header.substr(token_start, token_end - token_start);
                }
            }

            if (token.empty() || sessions.find(token) == sessions.end()) {
                return crow::response(401, "Invalid or expired session token");
            }
    
            //wip wip wip wip wip !!!!!
            User user = user_data[token];
            std::cout << user.getId() << std::endl;
            std::cout << combinedGenres << std::endl;
            std::string query = "UPDATE userGenres SET genres = '" + combinedGenres + "' WHERE user_id = " + user.getId() + ";";
            executeQuery(hDbc, query);
            
            


            crow::json::wvalue response;
            response["status"] = "success";
            response["message"] = "Genres saved successfully";
            return crow::response{response};
       
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
        auto query = req.url_params.get("query"); 

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

    CROW_ROUTE(app, "/talpaapiquery")
    .methods("GET"_method)
    ([](const crow::request& req) -> crow::response {
        std::string query = req.url_params.get("query");
        if (query.empty()) {
            return crow::response(400, R"({"error": "Query parameter is missing"})");
        }

        std::cout << "Query: " << query << std::endl;

        int limit = 10;
        int page = 1;
        std::string token = 
        std::string talpaApiUrl = "https://www.librarything.com/api/talpa.php";
        std::string encodedQuery = url_encode(query);

        std::stringstream urlStream;
        urlStream << talpaApiUrl
                  << "?token=" << token
                  << "&search=" << encodedQuery
                  << "&page=" << page
                  << "&limit=" << limit;

        std::string finalUrl = urlStream.str();
        std::string json_data = execute_python_script(finalUrl);

        Json::Value json_obj;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream json_stream(json_data);

        if (!Json::parseFromStream(reader, json_stream, &json_obj, &errs)) {
            std::cerr << "Error parsing JSON: " << errs << std::endl;
            return crow::response(500, R"({"error": "Failed to parse JSON data"})");
        }

        Json::Value titles;
        if (json_obj.isMember("response") && json_obj["response"].isMember("resultlist")) {
            const Json::Value& resultlist = json_obj["response"]["resultlist"];
            for (const auto& item : resultlist) {
                if (item.isMember("title")) {
                    titles.append(item["title"]);
                }
            }
        } else {
            return crow::response(500, R"({"error": "Invalid JSON structure"})");
        }

        Json::StreamWriterBuilder writer;
        std::string response_json = Json::writeString(writer, titles);

        crow::response res(response_json);
        res.set_header("Content-Type", "application/json");
        return res;
    });

    CROW_ROUTE(app, "/getbooks")
    .methods("POST"_method)
    ([](const crow::request& req) -> crow::response {
        Json::Value request_json;;
        Json::CharReaderBuilder reader;
        std::string errs;

        std::istringstream json_stream(req.body);
        if (!Json::parseFromStream(reader, json_stream, &request_json, &errs)) {
            std::cerr << "Error parsing JSON: " << errs << std::endl;
            return crow::response(400, R"({"error": "Invalid JSON data"})");
        }
        std::vector<std::string> titles;
        std::string query;
        if (request_json.isObject()) {
            if (request_json.isMember("titles") && request_json["titles"].isArray()) {
                for (const auto& title : request_json["titles"]) {
                    if (title.isString()) {
                        titles.push_back(title.asString());
                    }
                }
            }
            if (request_json.isMember("query") && request_json["query"].isString()) {
                query = request_json["query"].asString();
            }
        } else {
            return crow::response(400, R"({"error": "Invalid JSON structure, expecting an object with 'titles' and 'query' properties"})");
        }
        

         //getting cookies
         std::string token;
         auto cookies = req.headers.find("Cookie");
         if (cookies != req.headers.end()) {
             std::string cookie_header = cookies->second;
             size_t token_start = cookie_header.find("session_id=");
             if (token_start != std::string::npos) {
                 token_start += 11; 
                 size_t token_end = cookie_header.find(";", token_start);
                 token = cookie_header.substr(token_start, token_end - token_start);
             }
         }
        User user = user_data[token];
        std::string genreString = "SELECT genres FROM userGenres WHERE user_id = " + user.getId() + ";";
        std::string usergenres = executeQueryReturnString(hDbc, genreString);
        std::cout << usergenres << std::endl;
        std::vector<std::vector<std::string>> bookInfo;
        for(int i = 0; i < titles.size(); i++){
            std::string escapedTitle = escapeApostrophes(titles[i]);
            std::cout << escapedTitle << std::endl;
            std::string queryTalpa = "SELECT book_id, title, author, genre FROM books WHERE title LIKE '%" + escapedTitle + "%'";
            std::vector<std::vector<std::string>> row = executeQueryReturnRows(hDbc, queryTalpa);
            if(!row.empty()){
                for(int i = 0; i < row.size(); i++){
                    bookInfo.push_back(row[i]);
                }
            }
        }
        
        //WIP

        Json::Value responseJson;
        for (const auto& book : bookInfo) {
            Json::Value bookData;
            bookData["book_id"] = book[0]; 
            bookData["title"] = book[1];   
            bookData["author"] = book[2];   
            bookData["genre"] = book[3];   
            responseJson.append(bookData);
        }

        

        return crow::response(200, responseJson.toStyledString());
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

    CROW_ROUTE(app, "/book_details_bot.html")([](){
        crow::mustache::context ctx;
        return crow::response(crow::mustache::load("book_details_bot.html").render(ctx));
    });
    
    CROW_ROUTE(app, "/deleteBook")
    .methods("GET"_method, "DELETE"_method)
    ([](const crow::request& req) {
        std::string bookId = req.url_params.get("book_id");
        std::cout << bookId << std::endl;

        std::string query = "DELETE FROM books WHERE book_id = " + bookId + ";";
        executeQuery(hDbc, query);

        return crow::response(200, "Book deleted successfully");
    });

    app.route_dynamic("/upload_csv")
    .methods("POST"_method)([](const crow::request& req) {
        if (req.body.empty()) {
            return crow::response(400, "No file uploaded.");
        }


        std::string file_data = req.body;
        std::cout << "Request body: " << req.body << std::endl;
        std::string file_path = "..\\uploads\\uploaded_file.csv";
        std::cout << "Saving file to: " << file_path << std::endl;
        std::ofstream out(file_path, std::ios::binary);
        out.write(file_data.data(), file_data.size());
        out.close();
        std::cout << file_path << std::endl;
        insertCSV(file_path);

        return crow::response(200, "{\"message\": \"CSV data inserted successfully\"}");
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