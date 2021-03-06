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
#ifndef _FCITX_LIBIME_DICTIONARY_H_
#define _FCITX_LIBIME_DICTIONARY_H_

#include "libime_export.h"
#include "segmentgraph.h"
#include <boost/utility/string_view.hpp>
#include <functional>

namespace libime {

typedef std::function<void(const SegmentGraphPath &, boost::string_view, float,
                           boost::string_view)>
    GraphMatchCallback;

class LIBIME_EXPORT Dictionary {
public:
    void matchPrefix(
        const SegmentGraph &graph, GraphMatchCallback callback,
        const std::unordered_set<const SegmentGraphNode *> &ignore = {}) {
        matchPrefixImpl(graph, callback, ignore);
    }

protected:
    virtual void matchPrefixImpl(
        const SegmentGraph &graph, GraphMatchCallback callback,
        const std::unordered_set<const SegmentGraphNode *> &ignore) = 0;
};
}

#endif // _FCITX_LIBIME_DICTIONARY_H_
