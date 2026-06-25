using namespace std;

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>

const size_t PATTERN_NUM = 243; // 3^5 possible patterns for a 5-letter word
const int GREY = 0;
const int YELLOW = 1;
const int GREEN = 2;
const int SUGGESTION_NUM = 10;

array<int, PATTERN_NUM> patterns_for_guess(const string& guess, const vector<string>& candidate_words, const vector<array<int, 26>>& candidate_letter_counts) {
    array<int, PATTERN_NUM> pattern_counts{};

    for (size_t i = 0; i < candidate_words.size(); ++i) {
        const string& candidate = candidate_words[i];
        array<int, 26> letter_count = candidate_letter_counts[i];
        array<int, 5> pattern{};

        // Pass 1: lock in greens and consume those letters from the count.
        for (size_t j = 0; j < 5; j++) {
            if (guess[j] == candidate[j]) {
                pattern[j] = GREEN;
                letter_count[guess[j] - 'a']--;
            }
        }

        // Pass 2: assign yellows from whatever letters remain.
        for (size_t j = 0; j < 5; j++) {
            if (pattern[j] != GREEN && letter_count[guess[j] - 'a'] > 0) {
                pattern[j] = YELLOW;
                letter_count[guess[j] - 'a']--;
            }
        }

        // Pass 3: anything still un-set stays GREY (already initialised).

        int pattern_index = 0;
        int multiplier = 1;
        for (size_t j = 0; j < 5; j++) {
            pattern_index += pattern[j] * multiplier;
            multiplier *= 3;
        }

        pattern_counts[pattern_index]++;
    }

    return pattern_counts;
}

float computeGuessEntropy(const string& guess, const vector<string>& candidate_words, const vector<array<int, 26>>& candidate_letter_counts, size_t num_candidates, const vector<float>& log2_table) {
    float entropy = 0.0f;
    array<int, PATTERN_NUM> pattern_counts = patterns_for_guess(guess, candidate_words, candidate_letter_counts);
    float log2N = log2_table[num_candidates];
    for (int count : pattern_counts) {
        if (count > 0) {
            float probability = static_cast<float>(count) / num_candidates;
            entropy -= probability * (log2_table[count] - log2N);
        }
    }
    return entropy;
}

vector<pair<float, string>> rankGuesses(const vector<string>& all_words, const vector<string>& candidate_words, const vector<array<int, 26>>& candidate_letter_counts, int num_suggestions) {
    vector<pair<float, string>> scored_words;
    size_t num_candidates = candidate_words.size();

    vector<float> log2_table(num_candidates + 1);
    for (size_t c = 1; c <= num_candidates; ++c) log2_table[c] = log2((float)c);

    for (const auto& guess : all_words) {
        float entropy = computeGuessEntropy(guess, candidate_words, candidate_letter_counts, num_candidates, log2_table);
        scored_words.emplace_back(entropy, guess);
    }

    int k = min(num_suggestions, static_cast<int>(scored_words.size()));

    partial_sort(scored_words.begin(), scored_words.begin() + k, scored_words.end(), greater<pair<float, string>>());

    scored_words.resize(k);

    return scored_words;
}

vector<string> readFile(const string& filename) {
    ifstream file(filename);

    if (!file) {
        cerr << "Failed to open file\n";
        return {};
    }

    vector<string> words;
    string line;

    while (getline(file, line)) {
        if (!line.empty()) {
            words.push_back(line);
        }
    }

    return words;
}

vector<array<int, 26>> getLetterCounts(const vector<string>& words) {
    vector<array<int, 26>> letter_counts(words.size());
    for (auto& counts : letter_counts) counts.fill(0);

    for (size_t i = 0; i < words.size(); ++i) {
        for (char c : words[i]) {
            letter_counts[i][c - 'a']++;
        }
    }

    return letter_counts;
}

string promptGuess() {
    while (true) {
        string guess;

        cout << "Enter the word you guessed: ";
        cin >> guess;

        transform(guess.begin(), guess.end(), guess.begin(),
                  [](unsigned char c) { return tolower(c); });

        bool isAlpha = all_of(guess.begin(), guess.end(),
                              [](unsigned char c) { return isalpha(c); });

        if (guess.length() == 5 && isAlpha) {
            return guess;
        }

        cout << "Please enter a valid 5-letter word." << endl;
    }
}

vector<int> promptPattern() {
    while (true) {
        string raw;

        cout << "Enter the result (0=grey, 1=yellow, 2=green), e.g. 00102: ";
        cin >> raw;

        bool isValid = raw.length() == 5;

        for (char c : raw) {
            if (c != '0' && c != '1' && c != '2') {
                isValid = false;
                break;
            }
        }

        if (isValid) {
            vector<int> pattern;

            for (char c : raw) {
                pattern.push_back(c - '0');
            }

            return pattern;
        }

        cout << "Please enter exactly 5 digits, each 0, 1, or 2." << endl;
    }
}

void filter_candidates(const string& guess, const vector<int>& pattern_digits, vector<string>& candidate_words, vector<array<int, 26>>& candidate_letter_counts) {
    vector<string> new_candidates;
    vector<array<int, 26>> new_letter_counts;

    for (size_t i = 0; i < candidate_words.size(); ++i) {
        const string& candidate = candidate_words[i];
        array<int, 26> letter_count = candidate_letter_counts[i];

        bool matches = true;

        // Pass 1: greens must match exactly; consume those letters from the count.
        for (size_t j = 0; j < 5 && matches; j++) {
            if (pattern_digits[j] == GREEN) {
                if (guess[j] != candidate[j]) {
                    matches = false;
                } else {
                    letter_count[guess[j] - 'a']--;
                }
            }
        }

        // Pass 2: yellows must not be in the same position and must have a
        // remaining occurrence to claim. Decrement so later greys/yellows see
        // the correct leftover count.
        for (size_t j = 0; j < 5 && matches; j++) {
            if (pattern_digits[j] == YELLOW) {
                if (guess[j] == candidate[j] || letter_count[guess[j] - 'a'] <= 0) {
                    matches = false;
                } else {
                    letter_count[guess[j] - 'a']--;
                }
            }
        }

        // Pass 3: greys are only valid if no occurrences of that letter remain
        // after greens and yellows have claimed theirs.
        for (size_t j = 0; j < 5 && matches; j++) {
            if (pattern_digits[j] == GREY && letter_count[guess[j] - 'a'] > 0) {
                matches = false;
            }
        }

        if (matches) {
            new_candidates.push_back(candidate);
            new_letter_counts.push_back(candidate_letter_counts[i]);
        }
    }

    candidate_words = move(new_candidates);
    candidate_letter_counts = move(new_letter_counts);
}

int main() {
    vector<string> words = readFile("valid-wordle-words.txt");
    vector<array<int, 26>> letter_counts = getLetterCounts(words);

    vector<string> candidate_words = words;
    vector<array<int, 26>> candidate_letter_counts = letter_counts;

    int round = 1;

    while (true) {
        cout << "Round " << round << ": " << candidate_words.size() << " possible words remain." << endl;
        if (candidate_words.size() == 0) {
            cout << "No words match the feedback given - check your inputs for typos." << endl;
            break;
        } else if (candidate_words.size() == 1) {
            cout << "The word is: " << candidate_words[0] << endl;
            break;
        }

        vector<pair<float, string>> suggestions = rankGuesses(words, candidate_words, candidate_letter_counts, SUGGESTION_NUM);
        
        cout << "Top guesses by expected information gain:" << endl;
        for (size_t i = 0; i < suggestions.size(); i++) {
            cout << i << ": " << suggestions[i].second << " (entropy: " << suggestions[i].first << ")" << endl;
        }

        string guess = promptGuess();
        vector<int> pattern_digits = promptPattern();

        filter_candidates(guess, pattern_digits, candidate_words, candidate_letter_counts);

        round++;
    }

}