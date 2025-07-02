#define NOMINMAX
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <sqlite3.h>
#include <regex>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>

using namespace std;

// Lock program to 1 CPU core (sequential behavior)
void lock_to_one_cpu() {
    HANDLE process = GetCurrentProcess();
    DWORD_PTR mask = 1;
    SetProcessAffinityMask(process, mask);
    cout << "⚙️  Running on a single core..." << endl;
}

// Struct to hold raw database rows
struct RawSteamRow {
    std::string url;
    std::string types;
    std::string name;
    std::string desc_snippet;
    std::string recent_reviews;
    std::string all_reviews;
    std::string release_date;
    std::string developer;
    std::string publisher;
    std::string popular_tags;
    std::string game_details;
    std::string languages;
    std::string achievements;
    std::string genre;
    std::string game_description;
    std::string mature_content;
    std::string minimum_requirements;
    std::string recommended_requirements;
    std::string original_price;
    std::string discount_price;
};


// Part 2: SQLite Connection + Raw Row Loader

// Global database connection
sqlite3* db;

// Vector to hold all imported rows
vector<RawSteamRow> rawRows;

// Safe string reader to prevent null crashes
string get_text(sqlite3_stmt* stmt, int col) {
    const unsigned char* val = sqlite3_column_text(stmt, col);
    return val ? reinterpret_cast<const char*>(val) : "";
}

// Function to load all rows from the 'steam_games' table
bool load_raw_rows() {
    string query = "SELECT * FROM steam_games;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "❌ Failed to prepare SELECT: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RawSteamRow row;
        row.url                      = get_text(stmt, 0);
        row.types                    = get_text(stmt, 1);
        row.name                     = get_text(stmt, 2);
        row.desc_snippet             = get_text(stmt, 3);
        row.recent_reviews           = get_text(stmt, 4);
        row.all_reviews              = get_text(stmt, 5);
        row.release_date             = get_text(stmt, 6);
        row.developer                = get_text(stmt, 7);
        row.publisher                = get_text(stmt, 8);
        row.popular_tags             = get_text(stmt, 9);
        row.game_details             = get_text(stmt, 10);
        row.languages                = get_text(stmt, 11);
        row.achievements             = get_text(stmt, 12);
        row.genre                    = get_text(stmt, 13);
        row.game_description         = get_text(stmt, 14);
        row.mature_content           = get_text(stmt, 15);
        row.minimum_requirements     = get_text(stmt, 16);
        row.recommended_requirements = get_text(stmt, 17);
        row.original_price           = get_text(stmt, 18);
        row.discount_price           = get_text(stmt, 19);

        rawRows.push_back(row);
    }

    sqlite3_finalize(stmt);
    cout << "✅ Loaded " << rawRows.size() << " rows from the database.\n";
    return true;
}

// Utility to escape quotes and wrap field in quotes for CSV
string escape_csv(const string& input) {
    string output = input;
    size_t pos = 0;
    while ((pos = output.find("\"", pos)) != string::npos) {
        output.insert(pos, "\"");  // Double the quote
        pos += 2;
    }
    return "\"" + output + "\"";
}

// Debugging export to verify raw data
void export_raw_debug() {
    ofstream file("debug_raw_rows.csv");
    file << "url,types,name,desc_snippet,recent_reviews,all_reviews,release_date,developer,publisher,"
         << "popular_tags,game_details,languages,achievements,genre,game_description,mature_content,"
         << "minimum_requirements,recommended_requirements,original_price,discount_price\n";

    for (const auto& row : rawRows) {
        file << escape_csv(row.url) << ","
             << escape_csv(row.types) << ","
             << escape_csv(row.name) << ","
             << escape_csv(row.desc_snippet) << ","
             << escape_csv(row.recent_reviews) << ","
             << escape_csv(row.all_reviews) << ","
             << escape_csv(row.release_date) << ","
             << escape_csv(row.developer) << ","
             << escape_csv(row.publisher) << ","
             << escape_csv(row.popular_tags) << ","
             << escape_csv(row.game_details) << ","
             << escape_csv(row.languages) << ","
             << escape_csv(row.achievements) << ","
             << escape_csv(row.genre) << ","
             << escape_csv(row.game_description) << ","
             << escape_csv(row.mature_content) << ","
             << escape_csv(row.minimum_requirements) << ","
             << escape_csv(row.recommended_requirements) << ","
             << escape_csv(row.original_price) << ","
             << escape_csv(row.discount_price) << "\n";
    }

    file.close();
    cout << "📁 Raw row export saved to debug_raw_rows.csv\n";
}

int main() {
    lock_to_one_cpu();

    // Open the SQLite database
    int rc = sqlite3_open("steam.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "❌ Failed to open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }
    cout << "📂 Connected to steam.db successfully.\n";

    // Load and export rows
    if (load_raw_rows()) {
        export_raw_debug();
    } else {
        cerr << "⚠️  No data loaded from the database.\n";
    }

    // Close the database
    sqlite3_close(db);
    return 0;
}
