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
#include <unordered_map>

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
// ===================== Part 3: Struct Definitions & Data Formatter =====================

struct SteamGame {
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
    std::string price_str;

    float original_price = -1.0f;
    float all_reviews_percent = -1.0f;
    float recent_reviews_percent = -1.0f;
    std::string overall_genre;
};

std::vector<SteamGame> structured_games;

// Merge genre fields
std::string merge_genres(const std::string& tags, const std::string& details, const std::string& genre) {
    std::set<std::string> genre_set;
    std::stringstream ss(tags + "," + details + "," + genre);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        if (!token.empty()) genre_set.insert(token);
    }

    std::string result;
    for (const auto& g : genre_set) {
        if (!result.empty()) result += ", ";
        result += g;
    }
    return result;
}

// Extract the last number before a '%' symbol
float extract_review_percent(const std::string& text) {
    size_t pos = text.rfind('%');
    if (pos == std::string::npos) return -1.0f;

    size_t start = pos;
    while (start > 0 && (isdigit(text[start - 1]) || text[start - 1] == '.')) {
        start--;
    }

    std::string num = text.substr(start, pos - start);
    try {
        return std::stof(num);
    } catch (...) {
        return -1.0f;
    }
}

// Convert RawSteamRow → SteamGame with validation
void format_all_games() {
    structured_games.clear();

    for (const auto& row : rawRows) {
        try {
            SteamGame game;
            game.url                      = row.url;
            game.types                    = row.types;
            game.name                     = row.name;
            game.desc_snippet             = row.desc_snippet;
            game.recent_reviews           = row.recent_reviews;
            game.all_reviews              = row.all_reviews;
            game.release_date             = row.release_date;
            game.developer                = row.developer;
            game.publisher                = row.publisher;
            game.popular_tags             = row.popular_tags;
            game.game_details             = row.game_details;
            game.languages                = row.languages;
            game.achievements             = row.achievements;
            game.genre                    = row.genre;
            game.game_description         = row.game_description;
            game.mature_content           = row.mature_content;
            game.minimum_requirements     = row.minimum_requirements;
            game.recommended_requirements = row.recommended_requirements;
            game.price_str                = row.original_price;

            // Parse cleaned values
            game.all_reviews_percent = extract_review_percent(row.all_reviews);
            game.recent_reviews_percent = extract_review_percent(row.recent_reviews);
            game.overall_genre = merge_genres(row.popular_tags, row.game_details, row.genre);

            // Parse price
            std::string price = row.original_price;
            std::transform(price.begin(), price.end(), price.begin(), ::tolower);
            if (price.empty()) continue;

            if (price.find("free") != std::string::npos) {
                game.original_price = 0.0f;
            } else {
                price.erase(std::remove_if(price.begin(), price.end(), [](char c) {
                    return !(isdigit(c) || c == '.' || c == '-');
                }), price.end());

                if (price.empty()) continue;

                try {
                    game.original_price = std::stof(price);
                } catch (...) {
                    continue;
                }
            }

            structured_games.push_back(game);
        } catch (...) {
            continue;
        }
    }

    std::cout << "✅ Structured " << structured_games.size() << " games successfully.\n";
}

// Export only cleaned + formatted data
void export_structured_debug(const std::string& filename = "formatted_debug.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Could not open " << filename << " for writing.\n";
        return;
    }

    file << "name,release_date,developer,publisher,original_price,all_reviews_percent,recent_reviews_percent,overall_genre,languages,min_requirements,rec_requirements\n";

    for (const auto& game : structured_games) {
        file << escape_csv(game.name) << ","
             << escape_csv(game.release_date) << ","
             << escape_csv(game.developer) << ","
             << escape_csv(game.publisher) << ","
             << game.original_price << ","
             << game.all_reviews_percent << ","
             << game.recent_reviews_percent << ","
             << escape_csv(game.overall_genre) << ","
             << escape_csv(game.languages) << ","
             << escape_csv(game.minimum_requirements) << ","
             << escape_csv(game.recommended_requirements) << "\n";
    }

    file.close();
    std::cout << "📁 Structured export saved to " << filename << "\n";
}
// ===================== Part 4: System Requirements Analyzer =====================

struct SystemSpec {
    std::string os;
    std::string cpu;
    std::string gpu;
    int ram_gb = 0;
    int storage_gb = 0;
};

// Extract RAM in GB
int extract_ram(const std::string& text) {
    std::regex ram_pattern(R"((\d{1,3})\s*(GB|Mb|MB|gb|mb))");
    std::smatch match;
    if (std::regex_search(text, match, ram_pattern)) {
        try {
            int value = std::stoi(match[1].str());
            std::string unit = match[2].str();
            if (unit == "MB" || unit == "mb" || unit == "Mb") value /= 1024;
            if (value > 0 && value <= 256) return value;  // Limit to 256 GB
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// Extract Storage in GB
int extract_storage(const std::string& text) {
    std::regex storage_pattern(R"((\d{1,4})\s*(GB|Gb|MB|Mb))");
    std::smatch match;
    if (std::regex_search(text, match, storage_pattern)) {
        try {
            int value = std::stoi(match[1].str());
            std::string unit = match[2].str();
            if (unit == "MB" || unit == "mb") value /= 1024;
            if (value > 0 && value <= 2000) return value;  // Limit to 2TB
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// Generic helper: find line containing keyword
std::string extract_line(const std::string& text, const std::string& key) {
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find(key) != std::string::npos) {
            return line;
        }
    }
    return "";
}

// Extract all specs from a single text blob
SystemSpec parse_spec_block(const std::string& block) {
    SystemSpec spec;
    std::string line;

    line = extract_line(block, "OS");
    if (!line.empty()) spec.os = line;

    line = extract_line(block, "Processor");
    if (line.empty()) line = extract_line(block, "CPU");
    if (!line.empty()) spec.cpu = line;

    line = extract_line(block, "Graphics");
    if (line.empty()) line = extract_line(block, "GPU");
    if (!line.empty()) spec.gpu = line;

    spec.ram_gb = extract_ram(block);
    spec.storage_gb = extract_storage(block);

    return spec;
}

// Compare and update with most demanding values
void take_max(SystemSpec& base, const SystemSpec& current) {
    if (base.ram_gb < current.ram_gb) base.ram_gb = current.ram_gb;
    if (base.storage_gb < current.storage_gb) base.storage_gb = current.storage_gb;
    if (base.os.empty() || current.os > base.os) base.os = current.os;
    if (base.cpu.empty() || current.cpu > base.cpu) base.cpu = current.cpu;
    if (base.gpu.empty() || current.gpu > base.gpu) base.gpu = current.gpu;
}

// Final system specs
SystemSpec min_required_system;
SystemSpec rec_required_system;

// Analyze all games
void analyze_system_requirements() {
    min_required_system = {};
    rec_required_system = {};

    for (const auto& game : structured_games) {
        SystemSpec min = parse_spec_block(game.minimum_requirements);
        SystemSpec rec = parse_spec_block(game.recommended_requirements);

        take_max(min_required_system, min);
        take_max(rec_required_system, rec);
    }

    /*std::cout << "\nMinimum System Requirements:\n"
              << "   OS:      " << min_required_system.os << "\n"
              << "   CPU:     " << min_required_system.cpu << "\n"
              << "   GPU:     " << min_required_system.gpu << "\n"
              << "   RAM:     " << min_required_system.ram_gb << " GB\n"
              << "   Storage: " << min_required_system.storage_gb << " GB\n";

    std::cout << "\nRecommended System Requirements:\n"
              << "   OS:      " << rec_required_system.os << "\n"
              << "   CPU:     " << rec_required_system.cpu << "\n"
              << "   GPU:     " << rec_required_system.gpu << "\n"
              << "   RAM:     " << rec_required_system.ram_gb << " GB\n"
              << "   Storage: " << rec_required_system.storage_gb << " GB\n";*/
}

// Optional: Export to CSV
void export_requirements_debug(const std::string& filename = "system_requirements_summary.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Could not open " << filename << " for writing.\n";
        return;
    }

    file << "Type,OS,CPU,GPU,RAM (GB),Storage (GB)\n";
    file << "Minimum,"
         << escape_csv(min_required_system.os) << ","
         << escape_csv(min_required_system.cpu) << ","
         << escape_csv(min_required_system.gpu) << ","
         << min_required_system.ram_gb << ","
         << min_required_system.storage_gb << "\n";

    file << "Recommended,"
         << escape_csv(rec_required_system.os) << ","
         << escape_csv(rec_required_system.cpu) << ","
         << escape_csv(rec_required_system.gpu) << ","
         << rec_required_system.ram_gb << ","
         << rec_required_system.storage_gb << "\n";

    file.close();
    std::cout << "📄 System requirements summary saved to " << filename << "\n";
}
// ===================== Part 5: Top Games & Genres Analyzer =====================

struct TopGame {
    std::string name;
    std::string developer;
    float rating;
    float price;
    std::string release_date;
};

// Global containers
std::vector<TopGame> top_games;
std::vector<std::pair<std::string, int>> top_genres;

// Generate all games tied at highest rating
void compute_top_games() {
    std::vector<TopGame> all;
    for (const auto& game : structured_games) {
        if (game.all_reviews_percent < 0 || game.name.empty()) continue;

        TopGame g;
        g.name = game.name;
        g.developer = game.developer;
        g.rating = game.all_reviews_percent;
        g.price = game.original_price;
        g.release_date = game.release_date;
        all.push_back(g);
    }

    std::sort(all.begin(), all.end(), [](const TopGame& a, const TopGame& b) {
        return a.rating > b.rating;
    });

    top_games.clear();
    float max_rating = all.empty() ? -1.0f : all[0].rating;

    for (const auto& g : all) {
        if (g.rating == max_rating) {
            top_games.push_back(g);
        } else {
            break;
        }
    }
}

// Generate Top 5 most common genres
void compute_top_genres() {
    std::unordered_map<std::string, int> genre_count;

    for (const auto& game : structured_games) {
        std::stringstream ss(game.overall_genre);
        std::string genre;
        while (std::getline(ss, genre, ',')) {
            genre.erase(std::remove_if(genre.begin(), genre.end(), ::isspace), genre.end());
            if (!genre.empty()) genre_count[genre]++;
        }
    }

    std::vector<std::pair<std::string, int>> genre_list(genre_count.begin(), genre_count.end());
    std::sort(genre_list.begin(), genre_list.end(), [](auto& a, auto& b) {
        return b.second < a.second;
    });

    top_genres.assign(genre_list.begin(), genre_list.begin() + std::min<size_t>(5, genre_list.size()));
}

// Print Top games and genres
/*void print_top_games_and_genres() {
    std::cout << "\n🏆 Top Rated Games (All tied at " 
              << (top_games.empty() ? 0 : top_games[0].rating) << "%):\n";
    for (int i = 0; i < top_games.size(); ++i) {
        const auto& g = top_games[i];
        std::cout << i + 1 << ". " << g.name << " (" << g.developer << ") – "
                  << g.rating << "% – $" << g.price << " – " << g.release_date << "\n";
    }

    std::cout << "\n🎭 Top 5 Most Common Genres:\n";
    for (int i = 0; i < top_genres.size(); ++i) {
        std::cout << i + 1 << ". " << top_genres[i].first << " – " << top_genres[i].second << " games\n";
    }
}*/

// Export top-rated games to CSV
void export_top_games(const std::string& filename = "top_5_games.csv") {
    std::ofstream file(filename);
    file << "Name,Developer,Rating,Price,ReleaseDate\n";
    for (const auto& g : top_games) {
        file << escape_csv(g.name) << ","
             << escape_csv(g.developer) << ","
             << g.rating << ","
             << g.price << ","
             << escape_csv(g.release_date) << "\n";
    }
    file.close();
    std::cout << "📄 Top games saved to " << filename << "\n";
}

// Export top genres to CSV
void export_top_genres(const std::string& filename = "top_5_genres.csv") {
    std::ofstream file(filename);
    file << "Genre,Count\n";
    for (const auto& g : top_genres) {
        file << escape_csv(g.first) << "," << g.second << "\n";
    }
    file.close();
    std::cout << "📄 Top genres saved to " << filename << "\n";
}
// ===================== Part 6: Developer-Level Stats =====================

struct DeveloperStats {
    std::string developer;
    float avg_rating = 0.0f;
    float avg_price = 0.0f;
    std::string most_common_genre;
    std::string least_common_genre;
    std::string most_common_language;
    std::string least_common_language;
};

std::vector<DeveloperStats> developer_stats;

void compute_developer_stats() {
    std::unordered_map<std::string, std::vector<const SteamGame*>> dev_map;

    // Group all games by developer
    for (const auto& game : structured_games) {
        if (!game.developer.empty()) {
            dev_map[game.developer].push_back(&game);
        }
    }

    for (const auto& [dev, games] : dev_map) {
        if (games.empty()) continue;

        float rating_sum = 0.0f;
        int rating_count = 0;
        float price_sum = 0.0f;
        int price_count = 0;

        std::unordered_map<std::string, int> genre_count;
        std::unordered_map<std::string, int> lang_count;

        for (const auto* game : games) {
            // ✅ Use both recent and all_reviews for average rating
            float combined_rating = -1.0f;
            if (game->all_reviews_percent >= 0 && game->recent_reviews_percent >= 0)
                combined_rating = (game->all_reviews_percent + game->recent_reviews_percent) / 2;
            else if (game->all_reviews_percent >= 0)
                combined_rating = game->all_reviews_percent;
            else if (game->recent_reviews_percent >= 0)
                combined_rating = game->recent_reviews_percent;

            if (combined_rating >= 0) {
                rating_sum += combined_rating;
                rating_count++;
            }

            if (game->original_price >= 0) {
                price_sum += game->original_price;
                price_count++;
            }

            std::stringstream genre_ss(game->overall_genre);
            std::string genre;
            while (std::getline(genre_ss, genre, ',')) {
                genre.erase(std::remove_if(genre.begin(), genre.end(), ::isspace), genre.end());
                if (!genre.empty()) genre_count[genre]++;
            }

            std::stringstream lang_ss(game->languages);
            std::string lang;
            while (std::getline(lang_ss, lang, ',')) {
                lang.erase(std::remove_if(lang.begin(), lang.end(), ::isspace), lang.end());
                if (!lang.empty()) lang_count[lang]++;
            }
        }

        DeveloperStats stat;
        stat.developer = dev;
        stat.avg_rating = rating_count ? rating_sum / rating_count : 0.0f;
        stat.avg_price = price_count ? price_sum / price_count : 0.0f;

        if (!genre_count.empty()) {
            auto [max_it, min_it] = std::minmax_element(
                genre_count.begin(), genre_count.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            stat.most_common_genre = max_it->first;
            stat.least_common_genre = min_it->first;
        }

        if (!lang_count.empty()) {
            auto [max_it, min_it] = std::minmax_element(
                lang_count.begin(), lang_count.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            stat.most_common_language = max_it->first;
            stat.least_common_language = min_it->first;
        }

        developer_stats.push_back(stat);
    }

    std::cout << "📊 Computed developer-level stats for " << developer_stats.size() << " developers.\n";
}

void export_developer_stats(const std::string& filename = "developer_stats.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open developer_stats.csv\n";
        return;
    }

    file << "Developer,AvgRating,AvgPrice,MostCommonGenre,LeastCommonGenre,MostCommonLanguage,LeastCommonLanguage\n";
    for (const auto& d : developer_stats) {
        file << escape_csv(d.developer) << ","
             << d.avg_rating << ","
             << d.avg_price << ","
             << escape_csv(d.most_common_genre) << ","
             << escape_csv(d.least_common_genre) << ","
             << escape_csv(d.most_common_language) << ","
             << escape_csv(d.least_common_language) << "\n";
    }

    file.close();
    std::cout << "📄 Developer stats exported to " << filename << "\n";
}
// ===================== Part 7: Publisher-Level Stats =====================

struct PublisherStats {
    std::string publisher;
    float avg_rating = 0.0f;
    float avg_price = 0.0f;
    std::string most_common_genre;
    std::string least_common_genre;
    std::string most_common_language;
    std::string least_common_language;
};

std::vector<PublisherStats> publisher_stats;

void compute_publisher_stats() {
    std::unordered_map<std::string, std::vector<const SteamGame*>> pub_map;

    for (const auto& game : structured_games) {
        if (!game.publisher.empty()) {
            pub_map[game.publisher].push_back(&game);
        }
    }

    for (const auto& [pub, games] : pub_map) {
        if (games.empty()) continue;

        float rating_sum = 0.0f;
        int rating_count = 0;
        float price_sum = 0.0f;
        int price_count = 0;

        std::unordered_map<std::string, int> genre_count;
        std::unordered_map<std::string, int> lang_count;

        for (const auto* game : games) {
            float combined_rating = -1.0f;
            if (game->all_reviews_percent >= 0 && game->recent_reviews_percent >= 0)
                combined_rating = (game->all_reviews_percent + game->recent_reviews_percent) / 2;
            else if (game->all_reviews_percent >= 0)
                combined_rating = game->all_reviews_percent;
            else if (game->recent_reviews_percent >= 0)
                combined_rating = game->recent_reviews_percent;

            if (combined_rating >= 0) {
                rating_sum += combined_rating;
                rating_count++;
            }

            if (game->original_price >= 0) {
                price_sum += game->original_price;
                price_count++;
            }

            std::stringstream genre_ss(game->overall_genre);
            std::string genre;
            while (std::getline(genre_ss, genre, ',')) {
                genre.erase(std::remove_if(genre.begin(), genre.end(), ::isspace), genre.end());
                if (!genre.empty()) genre_count[genre]++;
            }

            std::stringstream lang_ss(game->languages);
            std::string lang;
            while (std::getline(lang_ss, lang, ',')) {
                lang.erase(std::remove_if(lang.begin(), lang.end(), ::isspace), lang.end());
                if (!lang.empty()) lang_count[lang]++;
            }
        }

        PublisherStats stat;
        stat.publisher = pub;
        stat.avg_rating = rating_count ? rating_sum / rating_count : 0.0f;
        stat.avg_price = price_count ? price_sum / price_count : 0.0f;

        if (!genre_count.empty()) {
            auto [max_it, min_it] = std::minmax_element(
                genre_count.begin(), genre_count.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            stat.most_common_genre = max_it->first;
            stat.least_common_genre = min_it->first;
        }

        if (!lang_count.empty()) {
            auto [max_it, min_it] = std::minmax_element(
                lang_count.begin(), lang_count.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            stat.most_common_language = max_it->first;
            stat.least_common_language = min_it->first;
        }

        publisher_stats.push_back(stat);
    }

    std::cout << "📊 Computed publisher-level stats for " << publisher_stats.size() << " publishers.\n";
}

void export_publisher_stats(const std::string& filename = "publisher_stats.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open publisher_stats.csv\n";
        return;
    }

    file << "Publisher,AvgRating,AvgPrice,MostCommonGenre,LeastCommonGenre,MostCommonLanguage,LeastCommonLanguage\n";
    for (const auto& p : publisher_stats) {
        file << escape_csv(p.publisher) << ","
             << p.avg_rating << ","
             << p.avg_price << ","
             << escape_csv(p.most_common_genre) << ","
             << escape_csv(p.least_common_genre) << ","
             << escape_csv(p.most_common_language) << ","
             << escape_csv(p.least_common_language) << "\n";
    }

    file.close();
    std::cout << "📄 Publisher stats exported to " << filename << "\n";
}
// ===================== Part 8: Benchmarking =====================

#include <chrono>
#include <functional>
using namespace std::chrono;

struct BenchmarkEntry {
    std::string part;
    long long duration_ms;
};

std::vector<BenchmarkEntry> benchmark_log;

// ✅ Clean timing wrapper function
void benchmark(const std::string& label, const std::function<void()>& func) {
    auto start = high_resolution_clock::now();
    func();
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start).count();
    benchmark_log.push_back({label, duration});
    std::cout << "⏱️  " << label << ": " << duration << " ms\n";
}

// ✅ Export formatted benchmark report
void export_benchmark_summary(const std::string& filename = "benchmark_results.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open benchmark_results.csv\n";
        return;
    }

    // ⏱ Section 1: Timing
    file << "Timing Summary\n";
    file << "Part,Time (ms)\n";
    long long total_time = 0;

    for (const auto& entry : benchmark_log) {
        file << entry.part << "," << entry.duration_ms << "\n";
        if (entry.part != "Wall Clock Time")
            total_time += entry.duration_ms;
    }

    file << "Total Execution Time," << total_time << "\n";
    file << "\n";

    // 📈 Section 2: Performance Stats
    file << "Performance Summary\n";
    file << "Metric,Value\n";
    file << "Raw Rows," << rawRows.size() << "\n";
    file << "Total Games Processed," << structured_games.size() << "\n";

    if (total_time > 0) {
        double throughput = static_cast<double>(structured_games.size()) / (static_cast<double>(total_time) / 1000.0);
        file << "Throughput (games/sec)," << throughput << "\n";
    }

    file << "\n";

    // 📦 Section 3: Output Sizes
    file << "Output Size Stats\n";
    file << "Category,Count\n";

    std::set<std::string> unique_devs, unique_pubs, unique_genres;
    for (const auto& g : structured_games) {
        if (!g.developer.empty()) unique_devs.insert(g.developer);
        if (!g.publisher.empty()) unique_pubs.insert(g.publisher);

        std::stringstream ss(g.overall_genre);
        std::string genre;
        while (std::getline(ss, genre, ',')) {
            genre.erase(std::remove_if(genre.begin(), genre.end(), ::isspace), genre.end());
            if (!genre.empty()) unique_genres.insert(genre);
        }
    }

    file << "Unique Developers," << unique_devs.size() << "\n";
    file << "Unique Publishers," << unique_pubs.size() << "\n";
    file << "Unique Genres," << unique_genres.size() << "\n";

    file.close();
    std::cout << "📊 Benchmark results saved to " << filename << "\n";
}

/*int main() {
    lock_to_one_cpu();

    auto wall_start = high_resolution_clock::now();

    // Open the SQLite database
    int rc = sqlite3_open("steam.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "❌ Failed to open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }
    cout << "📂 Connected to steam.db successfully.\n";

    // Part 2: Load raw rows from database
    benchmark("load_raw_rows", []() {
        if (!load_raw_rows()) {
            cerr << "⚠️  No data loaded from the database.\n";
            sqlite3_close(db);
            exit(1);
        }
    });

    // Export raw for verification (not timed)
    export_raw_debug();

    // Part 3: Format raw → structured
    benchmark("format_all_games", []() {
        format_all_games();
    });

    // Export structured result (not timed)
    export_structured_debug();

    // Part 4: System Requirements Analysis
    benchmark("analyze_system_requirements", []() {
        analyze_system_requirements();
    });
    export_requirements_debug();

    // Part 5: Top Games and Genres
    benchmark("compute_top_games", []() {
        compute_top_games();
    });
    benchmark("compute_top_genres", []() {
        compute_top_genres();
    });
    // print_top_games_and_genres(); // optional
    export_top_games();
    export_top_genres();

    // Part 6: Developer Stats
    benchmark("compute_developer_stats", []() {
        compute_developer_stats();
    });
    export_developer_stats();

    // Part 7: Publisher Stats
    benchmark("compute_publisher_stats", []() {
        compute_publisher_stats();
    });
    export_publisher_stats();

    // Wall-clock timing + Part 8: Benchmark Export
    auto wall_end = high_resolution_clock::now();
    auto wall_duration = duration_cast<milliseconds>(wall_end - wall_start).count();
    benchmark_log.push_back({"Wall Clock Time", wall_duration});
    export_benchmark_summary();

    // Done
    sqlite3_close(db);
    cout << "✅ Program completed successfully.\n";
    return 0;
}*/

int main() {
    lock_to_one_cpu();  // Force single-core

    std::vector<int> limits = {1000, 2000, 5000, 10000, 20000, 30000, 40000};
    std::ofstream log_file("size_vs_time_log.csv");
    log_file << "Version,Input Size,Execution Time (ms),Wall Clock Time (ms)\n";

    for (int limit : limits) {
        benchmark_log.clear();
        std::cout << "\n📊 Running benchmark with LIMIT = " << limit << " rows...\n";

        int rc = sqlite3_open("steam.db", &db);
        if (rc != SQLITE_OK) {
            std::cerr << "❌ Failed to open database.\n";
            return 1;
        }

        auto wall_start = std::chrono::high_resolution_clock::now();

        // Load rows with limit
        benchmark("load_raw_rows", [limit]() {
            std::string query = "SELECT * FROM steam_games LIMIT " + std::to_string(limit) + ";";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "❌ Failed to prepare SELECT: " << sqlite3_errmsg(db) << std::endl;
                return;
            }

            rawRows.clear();
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                RawSteamRow row;
                row.url = get_text(stmt, 0);
                row.types = get_text(stmt, 1);
                row.name = get_text(stmt, 2);
                row.desc_snippet = get_text(stmt, 3);
                row.recent_reviews = get_text(stmt, 4);
                row.all_reviews = get_text(stmt, 5);
                row.release_date = get_text(stmt, 6);
                row.developer = get_text(stmt, 7);
                row.publisher = get_text(stmt, 8);
                row.popular_tags = get_text(stmt, 9);
                row.game_details = get_text(stmt, 10);
                row.languages = get_text(stmt, 11);
                row.achievements = get_text(stmt, 12);
                row.genre = get_text(stmt, 13);
                row.game_description = get_text(stmt, 14);
                row.mature_content = get_text(stmt, 15);
                row.minimum_requirements = get_text(stmt, 16);
                row.recommended_requirements = get_text(stmt, 17);
                row.original_price = get_text(stmt, 18);
                row.discount_price = get_text(stmt, 19);
                rawRows.push_back(row);
                if (rawRows.size() >= limit) break;
            }

            sqlite3_finalize(stmt);
        });

        benchmark("format_all_games", [] { format_all_games(); });
        benchmark("analyze_system_requirements", [] { analyze_system_requirements(); });
        benchmark("compute_top_games", [] { compute_top_games(); });
        benchmark("compute_top_genres", [] { compute_top_genres(); });
        benchmark("compute_developer_stats", [] { compute_developer_stats(); });
        benchmark("compute_publisher_stats", [] { compute_publisher_stats(); });

        auto wall_end = std::chrono::high_resolution_clock::now();
        long long exec_time = 0;
        for (const auto& entry : benchmark_log)
            exec_time += entry.duration_ms;

        long long wall_time = std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();
        log_file << "Sequential," << limit << "," << exec_time << "," << wall_time << "\n";

        sqlite3_close(db);
    }

    log_file.close();
    std::cout << "📄 Logged results to size_vs_time_log.csv\n";
    return 0;
}

