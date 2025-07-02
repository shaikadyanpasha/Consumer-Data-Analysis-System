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
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <cmath>

using namespace std;

// Optional: Detect parallel mode (for logs)
void show_cpu_info() {
    unsigned int threads = std::thread::hardware_concurrency();
    std::cout << "⚙️  Parallel Mode Activated\n";
    std::cout << "🧵 Detected Logical Threads:  " << threads << "\n";
    std::cout << "🧠 Logical Processor Count: " << threads << "\n";
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
// ===================== Part 3: Format Raw Rows into Structured Games =====================

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
    float original_price = -1.0f;
    float discount_price = -1.0f;
    float all_reviews_percent = -1.0f;
    float recent_reviews_percent = -1.0f;
    std::string overall_genre;
};

std::vector<SteamGame> structured_games;

// -- Extract rating percent (e.g., from "Very Positive (95%)")
float extract_review_percent(const std::string& input) {
    size_t percent_pos = input.rfind('%');
    if (percent_pos == std::string::npos) return -1.0f;
    size_t start = input.rfind('(', percent_pos);
    if (start == std::string::npos) return -1.0f;
    try {
        return std::stof(input.substr(start + 1, percent_pos - start - 1));
    } catch (...) {
        return -1.0f;
    }
}

// -- Parse price field, set "free" to 0
float parse_price(const std::string& price) {
    std::string lower = price;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("free") != std::string::npos) return 0.0f;
    try {
        return std::stof(lower);
    } catch (...) {
        return -1.0f;
    }
}

// -- Merge genre tags from 3 sources into deduplicated string
std::string merge_genres(const std::string& tags, const std::string& details, const std::string& genre) {
    std::set<std::string> all;
    std::stringstream ss(tags + "," + details + "," + genre);
    std::string token;
    while (std::getline(ss, token, ',')) {
        std::string trimmed;
        std::remove_copy_if(token.begin(), token.end(), std::back_inserter(trimmed), ::isspace);
        if (!trimmed.empty()) all.insert(trimmed);
    }

    std::string result;
    for (const auto& g : all) {
        if (!result.empty()) result += ", ";
        result += g;
    }
    return result;
}

// -- Thread worker to parse a chunk of raw rows
void parse_chunk(int start, int end, std::vector<SteamGame>& local) {
    for (int i = start; i < end; ++i) {
        const auto& row = rawRows[i];

        float original = parse_price(row.original_price);
        if (original < 0 || row.name.empty()) continue;

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
        game.original_price           = original;
        game.discount_price           = parse_price(row.discount_price);
        game.all_reviews_percent      = extract_review_percent(row.all_reviews);
        game.recent_reviews_percent   = extract_review_percent(row.recent_reviews);
        game.overall_genre            = merge_genres(row.popular_tags, row.game_details, row.genre);

        local.push_back(game);
    }
}

// -- Main formatter (parallelized)
void format_all_games() {
    structured_games.clear();

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    int total = rawRows.size();
    int chunk = (total + threads - 1) / threads;

    std::vector<std::thread> workers;
    std::vector<std::vector<SteamGame>> results(threads);

    for (unsigned int t = 0; t < threads; ++t) {
        int start = t * chunk;
        int end = std::min(start + chunk, total);
        workers.emplace_back(parse_chunk, start, end, std::ref(results[t]));
    }

    for (auto& w : workers) w.join();

    for (const auto& part : results)
        structured_games.insert(structured_games.end(), part.begin(), part.end());

    std::cout << "✅ Structured " << structured_games.size() << " games successfully.\n";
}
// ===================== Export Formatted Structured Rows =====================

void export_structured_debug(const std::string& filename = "formatted_debug.csv") {
    std::ofstream file(filename);
    file << "name,release_date,developer,publisher,original_price,discount_price,"
         << "all_reviews_percent,recent_reviews_percent,overall_genre,languages,types,achievements\n";

    for (const auto& g : structured_games) {
        file << escape_csv(g.name) << ","
             << escape_csv(g.release_date) << ","
             << escape_csv(g.developer) << ","
             << escape_csv(g.publisher) << ","
             << g.original_price << ","
             << g.discount_price << ","
             << g.all_reviews_percent << ","
             << g.recent_reviews_percent << ","
             << escape_csv(g.overall_genre) << ","
             << escape_csv(g.languages) << ","
             << escape_csv(g.types) << ","
             << escape_csv(g.achievements) << "\n";
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

SystemSpec min_required_system, rec_required_system;
std::mutex spec_mutex;

// -- Helper to extract a number (like "8 GB") from a string
int extract_number_gb(const std::string& s) {
    std::stringstream ss(s);
    std::string token;
    while (ss >> token) {
        try {
            float value = std::stof(token);
            if (s.find("GB") != std::string::npos || s.find("gb") != std::string::npos)
                return static_cast<int>(std::ceil(value));
        } catch (...) {}
    }
    return 0;
}

// -- Helper to parse each field for system requirements
void analyze_chunk(int start, int end, const std::vector<SteamGame>& games,
                   std::vector<SystemSpec>& min_local, std::vector<SystemSpec>& rec_local) {
    for (int i = start; i < end; ++i) {
        const auto& game = games[i];
        SystemSpec min, rec;

        // Extract known fields
        std::regex os_regex(R"(OS:\s*([^\n\r]+))");
        std::regex cpu_regex(R"(Processor:\s*([^\n\r]+))");
        std::regex gpu_regex(R"(Graphics:\s*([^\n\r]+))");
        std::regex ram_regex(R"(Memory:\s*([^\n\r]+))");
        std::regex storage_regex(R"(Storage:\s*([^\n\r]+))");

        std::smatch match;
        if (std::regex_search(game.minimum_requirements, match, os_regex))      min.os = match[1];
        if (std::regex_search(game.minimum_requirements, match, cpu_regex))     min.cpu = match[1];
        if (std::regex_search(game.minimum_requirements, match, gpu_regex))     min.gpu = match[1];
        if (std::regex_search(game.minimum_requirements, match, ram_regex))     min.ram_gb = extract_number_gb(match[1]);
        if (std::regex_search(game.minimum_requirements, match, storage_regex)) min.storage_gb = extract_number_gb(match[1]);

        if (std::regex_search(game.recommended_requirements, match, os_regex))      rec.os = match[1];
        if (std::regex_search(game.recommended_requirements, match, cpu_regex))     rec.cpu = match[1];
        if (std::regex_search(game.recommended_requirements, match, gpu_regex))     rec.gpu = match[1];
        if (std::regex_search(game.recommended_requirements, match, ram_regex))     rec.ram_gb = extract_number_gb(match[1]);
        if (std::regex_search(game.recommended_requirements, match, storage_regex)) rec.storage_gb = extract_number_gb(match[1]);

        min_local.push_back(min);
        rec_local.push_back(rec);
    }
}

// -- Helper to keep highest value per field
void take_max(SystemSpec& base, const SystemSpec& new_val) {
    if (new_val.os.length() > base.os.length()) base.os = new_val.os;
    if (new_val.cpu.length() > base.cpu.length()) base.cpu = new_val.cpu;
    if (new_val.gpu.length() > base.gpu.length()) base.gpu = new_val.gpu;
    if (new_val.ram_gb > base.ram_gb) base.ram_gb = new_val.ram_gb;
    if (new_val.storage_gb > base.storage_gb) base.storage_gb = new_val.storage_gb;
}

// -- Main analyzer
void analyze_system_requirements() {
    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    int total = structured_games.size();
    int chunk = (total + threads - 1) / threads;

    std::vector<std::thread> workers;
    std::vector<std::vector<SystemSpec>> local_min(threads), local_rec(threads);

    for (unsigned int t = 0; t < threads; ++t) {
        int start = t * chunk;
        int end = std::min(start + chunk, total);
        workers.emplace_back(analyze_chunk, start, end, std::ref(structured_games),
                             std::ref(local_min[t]), std::ref(local_rec[t]));
    }

    for (auto& w : workers) w.join();

    for (unsigned int t = 0; t < threads; ++t) {
        for (const auto& spec : local_min[t]) take_max(min_required_system, spec);
        for (const auto& spec : local_rec[t]) take_max(rec_required_system, spec);
    }

    std::cout << "\nMinimum System Requirements:\n";
    std::cout << "   OS:      " << min_required_system.os << "\n";
    std::cout << "   CPU:     " << min_required_system.cpu << "\n";
    std::cout << "   GPU:     " << min_required_system.gpu << "\n";
    std::cout << "   RAM:     " << min_required_system.ram_gb << " GB\n";
    std::cout << "   Storage: " << min_required_system.storage_gb << " GB\n";

    std::cout << "\nRecommended System Requirements:\n";
    std::cout << "   OS:      " << rec_required_system.os << "\n";
    std::cout << "   CPU:     " << rec_required_system.cpu << "\n";
    std::cout << "   GPU:     " << rec_required_system.gpu << "\n";
    std::cout << "   RAM:     " << rec_required_system.ram_gb << " GB\n";
    std::cout << "   Storage: " << rec_required_system.storage_gb << " GB\n";
}

// -- Export results
void export_requirements_debug(const std::string& filename = "system_requirements_summary.csv") {
    std::ofstream file(filename);
    file << "Field,Minimum,Recommended\n";
    file << "OS," << escape_csv(min_required_system.os) << "," << escape_csv(rec_required_system.os) << "\n";
    file << "CPU," << escape_csv(min_required_system.cpu) << "," << escape_csv(rec_required_system.cpu) << "\n";
    file << "GPU," << escape_csv(min_required_system.gpu) << "," << escape_csv(rec_required_system.gpu) << "\n";
    file << "RAM," << min_required_system.ram_gb << "," << rec_required_system.ram_gb << "\n";
    file << "Storage," << min_required_system.storage_gb << "," << rec_required_system.storage_gb << "\n";
    file.close();
    std::cout << "📄 System requirements summary saved to " << filename << "\n";
}
// ===================== Part 5: Top Games and Genres ===================== 

std::vector<SteamGame> top_games;
std::vector<std::string> top_genres;

void compute_top_games() {
    std::vector<SteamGame> all = structured_games;

    std::sort(all.begin(), all.end(), [](const SteamGame& a, const SteamGame& b) {
        return a.all_reviews_percent > b.all_reviews_percent;
    });

    // Always take all tied highest-rated games (e.g. all 100%)
    float top_rating = all.empty() ? -1.0f : all.front().all_reviews_percent;

    for (const auto& g : all) {
        if (g.all_reviews_percent == top_rating)
            top_games.push_back(g);
        else
            break;
    }

    std::cout << "🎮 Top Games with " << top_rating << "% rating: " << top_games.size() << " found.\n";
}

void compute_top_genres() {
    std::unordered_map<std::string, int> genre_count;

    for (const auto& g : structured_games) {
        std::stringstream ss(g.overall_genre);
        std::string genre;

        while (std::getline(ss, genre, ',')) {
            genre.erase(std::remove_if(genre.begin(), genre.end(), ::isspace), genre.end());
            if (!genre.empty()) genre_count[genre]++;
        }
    }

    std::vector<std::pair<std::string, int>> sorted(genre_count.begin(), genre_count.end());
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) {
        return a.second > b.second;
    });

    for (int i = 0; i < 5 && i < sorted.size(); ++i) {
        top_genres.push_back(sorted[i].first);
    }

    std::cout << "🎯 Top Genres:\n";
    for (const auto& g : top_genres)
        std::cout << " - " << g << " (" << genre_count[g] << " games)\n";
}

// -- Export functions
void export_top_games(const std::string& filename = "top_5_games.csv") {
    std::ofstream file(filename);
    file << "name,release_date,developer,publisher,all_reviews_percent\n";

    for (const auto& g : top_games) {
        file << escape_csv(g.name) << ","
             << escape_csv(g.release_date) << ","
             << escape_csv(g.developer) << ","
             << escape_csv(g.publisher) << ","
             << g.all_reviews_percent << "\n";
    }

    file.close();
    std::cout << "📄 Top 5 games export saved to " << filename << "\n";
}

void export_top_genres(const std::string& filename = "top_5_genres.csv") {
    std::ofstream file(filename);
    file << "genre\n";
    for (const auto& g : top_genres) {
        file << g << "\n";
    }
    file.close();
    std::cout << "📄 Top 5 genres export saved to " << filename << "\n";
}
// ===================== Part 6: Developer-Level Stats =====================

struct DeveloperStats {
    std::string developer;
    float avg_all = 0;
    float avg_recent = 0;
    float avg_price = 0;
    std::string common_genre;
    std::string least_common_genre;
    std::string common_language;
    std::string least_common_language;
    int count = 0;
};

std::vector<DeveloperStats> developer_stats;

// Helpers to find most/least common in a list
std::string most_common(const std::vector<std::string>& items) {
    std::map<std::string, int> freq;
    for (const auto& item : items)
        if (!item.empty()) freq[item]++;
    return freq.empty() ? "" : std::max_element(freq.begin(), freq.end(),
        [](auto& a, auto& b) { return a.second < b.second; })->first;
}

std::string least_common(const std::vector<std::string>& items) {
    std::map<std::string, int> freq;
    for (const auto& item : items)
        if (!item.empty()) freq[item]++;
    return freq.empty() ? "" : std::min_element(freq.begin(), freq.end(),
        [](auto& a, auto& b) { return a.second < b.second; })->first;
}

void compute_developer_stats() {
    developer_stats.clear();
    std::map<std::string, std::vector<SteamGame>> buckets;

    for (const auto& g : structured_games)
        if (!g.developer.empty())
            buckets[g.developer].push_back(g);

    std::vector<std::string> devs;
    for (const auto& [dev, _] : buckets)
        devs.push_back(dev);

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    int chunk = (devs.size() + threads - 1) / threads;
    std::vector<std::thread> workers;
    std::vector<std::vector<DeveloperStats>> local_results(threads);

    auto work = [&](int start, int end, std::vector<DeveloperStats>& local) {
        for (int i = start; i < end && i < devs.size(); ++i) {
            const auto& dev = devs[i];
            const auto& games = buckets[dev];
            if (games.empty()) continue;

            DeveloperStats stat;
            stat.developer = dev;
            std::vector<std::string> langs, genres;
            float sum_all = 0, sum_recent = 0, sum_price = 0;
            int count = 0;

            for (const auto& g : games) {
                if (g.all_reviews_percent >= 0) sum_all += g.all_reviews_percent;
                if (g.recent_reviews_percent >= 0) sum_recent += g.recent_reviews_percent;
                if (g.original_price >= 0) sum_price += g.original_price;
                langs.push_back(g.languages);
                genres.push_back(g.overall_genre);
                count++;
            }

            stat.count = count;
            stat.avg_all = sum_all / count;
            stat.avg_recent = sum_recent / count;
            stat.avg_price = sum_price / count;
            stat.common_genre = most_common(genres);
            stat.least_common_genre = least_common(genres);
            stat.common_language = most_common(langs);
            stat.least_common_language = least_common(langs);

            local.push_back(stat);
        }
    };

    for (unsigned int t = 0; t < threads; ++t) {
        int start = t * chunk;
        int end = std::min((int)devs.size(), start + chunk);
        workers.emplace_back(work, start, end, std::ref(local_results[t]));
    }

    for (auto& w : workers) w.join();

    for (const auto& chunk : local_results)
        developer_stats.insert(developer_stats.end(), chunk.begin(), chunk.end());

    std::cout << "📊 Developer stats computed: " << developer_stats.size() << "\n";
}

void export_developer_stats(const std::string& filename = "developer_stats.csv") {
    std::ofstream file(filename);
    file << "developer,avg_all,avg_recent,avg_price,common_genre,least_common_genre,common_language,least_common_language\n";

    for (const auto& s : developer_stats) {
        file << "\"" << s.developer << "\","
             << s.avg_all << ","
             << s.avg_recent << ","
             << s.avg_price << ","
             << "\"" << s.common_genre << "\","
             << "\"" << s.least_common_genre << "\","
             << "\"" << s.common_language << "\","
             << "\"" << s.least_common_language << "\"\n";
    }

    file.close();
    std::cout << "📄 Developer stats export saved to " << filename << "\n";
}
// ===================== Part 7: Publisher-Level Stats =====================

struct PublisherStats {
    std::string publisher;
    float avg_all = 0;
    float avg_recent = 0;
    float avg_price = 0;
    std::string common_genre;
    std::string least_common_genre;
    std::string common_language;
    std::string least_common_language;
    int count = 0;
};

std::vector<PublisherStats> publisher_stats;

void compute_publisher_stats() {
    publisher_stats.clear();
    std::map<std::string, std::vector<SteamGame>> buckets;

    for (const auto& g : structured_games)
        if (!g.publisher.empty())
            buckets[g.publisher].push_back(g);

    std::vector<std::string> pubs;
    for (const auto& [pub, _] : buckets)
        pubs.push_back(pub);

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    int chunk = (pubs.size() + threads - 1) / threads;
    std::vector<std::thread> workers;
    std::vector<std::vector<PublisherStats>> local_results(threads);

    auto work = [&](int start, int end, std::vector<PublisherStats>& local) {
        for (int i = start; i < end && i < pubs.size(); ++i) {
            const auto& pub = pubs[i];
            const auto& games = buckets[pub];
            if (games.empty()) continue;

            PublisherStats stat;
            stat.publisher = pub;
            std::vector<std::string> langs, genres;
            float sum_all = 0, sum_recent = 0, sum_price = 0;
            int count = 0;

            for (const auto& g : games) {
                if (g.all_reviews_percent >= 0) sum_all += g.all_reviews_percent;
                if (g.recent_reviews_percent >= 0) sum_recent += g.recent_reviews_percent;
                if (g.original_price >= 0) sum_price += g.original_price;
                langs.push_back(g.languages);
                genres.push_back(g.overall_genre);
                count++;
            }

            stat.count = count;
            stat.avg_all = sum_all / count;
            stat.avg_recent = sum_recent / count;
            stat.avg_price = sum_price / count;
            stat.common_genre = most_common(genres);
            stat.least_common_genre = least_common(genres);
            stat.common_language = most_common(langs);
            stat.least_common_language = least_common(langs);

            local.push_back(stat);
        }
    };

    for (unsigned int t = 0; t < threads; ++t) {
        int start = t * chunk;
        int end = std::min((int)pubs.size(), start + chunk);
        workers.emplace_back(work, start, end, std::ref(local_results[t]));
    }

    for (auto& w : workers) w.join();

    for (const auto& chunk : local_results)
        publisher_stats.insert(publisher_stats.end(), chunk.begin(), chunk.end());

    std::cout << "📊 Publisher stats computed: " << publisher_stats.size() << "\n";
}

void export_publisher_stats(const std::string& filename = "publisher_stats.csv") {
    std::ofstream file(filename);
    file << "publisher,avg_all,avg_recent,avg_price,common_genre,least_common_genre,common_language,least_common_language\n";

    for (const auto& s : publisher_stats) {
        file << "\"" << s.publisher << "\","
             << s.avg_all << ","
             << s.avg_recent << ","
             << s.avg_price << ","
             << "\"" << s.common_genre << "\","
             << "\"" << s.least_common_genre << "\","
             << "\"" << s.common_language << "\","
             << "\"" << s.least_common_language << "\"\n";
    }

    file.close();
    std::cout << "📄 Publisher stats export saved to " << filename << "\n";
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

// Time and log a stage
void benchmark(const std::string& label, const std::function<void()>& func) {
    auto start = high_resolution_clock::now();
    func();
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start).count();
    benchmark_log.push_back({label, duration});
    std::cout << "⏱️  " << label << ": " << duration << " ms\n";
}

// Export summary in the same format as sequential
void export_benchmark_summary(const std::string& filename = "benchmark_results.csv") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open " << filename << "\n";
        return;
    }

    // Section 1: Timing Summary
    file << "Timing Summary\n";
    file << "Part,Time (ms)\n";
    long long total = 0;
    for (const auto& entry : benchmark_log) {
        file << entry.part << "," << entry.duration_ms << "\n";
        if (entry.part != "Wall Clock Time") total += entry.duration_ms;
    }
    file << "Total Execution Time," << total << "\n\n";

    // Section 2: Performance Summary
    file << "Performance Summary\n";
    file << "Metric,Value\n";
    file << "Raw Rows," << rawRows.size() << "\n";
    file << "Total Games Processed," << structured_games.size() << "\n";
    if (total > 0) {
        double throughput = (double)structured_games.size() / (total / 1000.0);
        file << "Throughput (games/sec)," << throughput << "\n";
    }
    file << "\n";

    // Section 3: Output Size Stats
    file << "Output Size Stats\n";
    file << "Category,Count\n";

    std::set<std::string> devs, pubs, genres;
    for (const auto& g : structured_games) {
        if (!g.developer.empty()) devs.insert(g.developer);
        if (!g.publisher.empty()) pubs.insert(g.publisher);
        std::stringstream ss(g.overall_genre);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
            if (!token.empty()) genres.insert(token);
        }
    }
    file << "Unique Developers," << devs.size() << "\n";
    file << "Unique Publishers," << pubs.size() << "\n";
    file << "Unique Genres," << genres.size() << "\n";

    file.close();
    std::cout << "📊 Benchmark results saved to " << filename << "\n";
}
long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

/*int main() {
    show_cpu_info();  // From Part 1

    // Open the SQLite database
    int rc = sqlite3_open("steam.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "❌ Failed to open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    std::cout << "📂 Connected to steam.db successfully.\n";

    long long wall_start = now_ms();

    // Part 2: Load raw rows
    benchmark("load_raw_rows", [] {
        if (!load_raw_rows()) {
            std::cerr << "⚠️  No data loaded from the database.\n";
            exit(1);
        }
    });
    export_raw_debug();

    // Part 3: Format to structured games
    benchmark("format_all_games", [] {
        format_all_games();
    });
    export_structured_debug();

    // Part 4: System Requirements
    benchmark("analyze_system_requirements", [] {
        analyze_system_requirements();
    });
    export_requirements_debug();

    // Part 5: Top games and genres
    benchmark("compute_top_games", [] {
        compute_top_games();
    });
    benchmark("compute_top_genres", [] {
        compute_top_genres();
    });
    export_top_games();
    export_top_genres();

    // Part 6: Developer stats
    benchmark("compute_developer_stats", [] {
        compute_developer_stats();
    });
    export_developer_stats();

    // Part 7: Publisher stats
    benchmark("compute_publisher_stats", [] {
        compute_publisher_stats();
    });
    export_publisher_stats();

    // Part 8: Benchmark summary
    long long wall_end = now_ms();
    long long wall_time = wall_end - wall_start;
    benchmark_log.push_back({ "Wall Clock Time", wall_time });

    export_benchmark_summary();

    sqlite3_close(db);
    std::cout << "✅ Program completed successfully.\n";
    return 0;
}*/

int main() {
    show_cpu_info();  // From Part 1

    std::vector<int> limits = {1000, 2000, 5000, 10000, 20000, 30000, 40000};
    std::ofstream log_file("size_vs_time_log.csv");
    log_file << "Version,Input Size,Execution Time (ms),Wall Clock Time (ms)\n";

    for (int limit : limits) {
        benchmark_log.clear();
        std::cout << "\n📊 Running benchmark with LIMIT = " << limit << " rows...\n";

        // Open DB
        if (sqlite3_open("steam.db", &db) != SQLITE_OK) {
            std::cerr << "❌ Failed to open database.\n";
            return 1;
        }

        long long wall_start = now_ms();

        // Load limited rows
        benchmark("load_raw_rows", [limit] {
            std::string query = "SELECT * FROM steam_games LIMIT " + std::to_string(limit) + ";";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "❌ Failed SELECT\n";
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

        long long wall_end = now_ms();
        long long exec_time = 0;
        for (const auto& entry : benchmark_log)
            exec_time += entry.duration_ms;

        log_file << "Parallel," << limit << "," << exec_time << "," << (wall_end - wall_start) << "\n";
        sqlite3_close(db);
    }

    log_file.close();
    std::cout << "📄 Logged results to size_vs_time_log.csv\n";
    return 0;
}

