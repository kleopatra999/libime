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
#ifndef _FCITX_LIBIME_PINYINIME_H_
#define _FCITX_LIBIME_PINYINIME_H_

#include "libime_export.h"
#include "pinyinencoder.h"
#include <fcitx-utils/macros.h>
#include <limits>
#include <memory>

namespace libime {

class PinyinIMEPrivate;
class PinyinDecoder;
class PinyinDictionary;
class UserLanguageModel;

class LIBIME_EXPORT PinyinIME {
public:
    PinyinIME(std::unique_ptr<PinyinDictionary> dict,
              std::unique_ptr<UserLanguageModel> model);
    virtual ~PinyinIME();

    PinyinFuzzyFlags fuzzyFlags() const;
    void setFuzzyFlags(PinyinFuzzyFlags flags);
    size_t nbest() const;
    void setNBest(size_t n);
    size_t beamSize() const;
    void setBeamSize(size_t n);
    size_t frameSize() const;
    void setFrameSize(size_t n);
    void setScoreFilter(float maxDistance = std::numeric_limits<float>::max(),
                        float minPath = -std::numeric_limits<float>::max());

    float maxDistance() const;
    float minPath() const;

    PinyinDictionary *dict();
    const PinyinDictionary *dict() const;
    const PinyinDecoder *decoder() const;
    UserLanguageModel *model();
    const UserLanguageModel *model() const;

private:
    std::unique_ptr<PinyinIMEPrivate> d_ptr;
    FCITX_DECLARE_PRIVATE(PinyinIME);
};
}

#endif // _FCITX_LIBIME_PINYINIME_H_
