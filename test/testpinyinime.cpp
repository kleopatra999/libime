/*
 * Copyright (C) 2017~2017 by CSSlayer
 * wengxt@gmail.com
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "libime/historybigram.h"
#include "libime/lattice.h"
#include "libime/pinyincontext.h"
#include "libime/pinyindecoder.h"
#include "libime/pinyindictionary.h"
#include "libime/pinyinime.h"
#include "libime/userlanguagemodel.h"
#include <boost/range/adaptor/transformed.hpp>
#include <chrono>
#include <fcitx-utils/stringutils.h>
#include <functional>
#include <iostream>
#include <sstream>

using namespace libime;
struct ScopedNanoTimer {
    std::chrono::high_resolution_clock::time_point t0;
    std::function<void(int)> cb;

    ScopedNanoTimer(std::function<void(int)> callback)
        : t0(std::chrono::high_resolution_clock::now()), cb(callback) {}
    ~ScopedNanoTimer(void) {
        auto t1 = std::chrono::high_resolution_clock::now();
        auto nanos =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count();

        cb(nanos);
    }
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        return 1;
    }
    auto printTime = [](int t) {
        std::cout << "Time: " << t / 1000000.0 << " ms" << std::endl;
    };
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>(argv[2]));
    ime.setNBest(2);
    ime.dict()->load(PinyinDictionary::SystemDict, argv[1],
                     PinyinDictFormat::Binary);
    ime.setFuzzyFlags(PinyinFuzzyFlag::Inner);
    PinyinContext c(&ime);
    ime.setScoreFilter(3.0f);

    std::string word;
    while (std::cin >> word) {
        ScopedNanoTimer t(printTime);
        if (word == "back") {
            c.backspace();
        } else if (word == "reset") {
            c.clear();
        } else if (word.size() == 1 &&
                   (('a' <= word[0] && word[0] <= 'z') ||
                    (!c.userInput().empty() && word[0] == '\''))) {
            c.type(word);
        } else if (word.size() == 1 && ('0' <= word[0] && word[0] <= '9')) {
            size_t idx;
            if (word[0] == '0') {
                idx = 9;
            } else {
                idx = word[0] - '1';
            }
            if (c.candidates().size() >= idx) {
                c.select(idx);
            }
        }
        if (c.selected()) {
            std::cout << "COMMIT:   " << c.preedit() << std::endl;
            c.learn();
            c.clear();
            continue;
        }
        std::cout << "PREEDIT:  " << c.preedit() << std::endl;
        std::cout << "SENTENCE: " << c.sentence() << std::endl;
        size_t count = 1;
        for (auto &candidate : c.candidates()) {
            std::cout << (count % 10) << ": ";
            for (auto node : candidate.sentence()) {
                auto &pinyin = static_cast<const PinyinLatticeNode *>(node)
                                   ->encodedPinyin();
                std::cout << node->word();
                if (!pinyin.empty()) {
                    std::cout << " " << PinyinEncoder::decodeFullPinyin(pinyin);
                }
            }
            std::cout << " " << candidate.score() << std::endl;
            count++;
            if (count > 10) {
                break;
            }
        }
    }

    ime.dict()->dump(PinyinDictionary::UserDict, std::cout);
    ime.model()->history().save(std::cout);

    return 0;
}
