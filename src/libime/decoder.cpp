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

#include "decoder.h"
#include "datrie.h"
#include "languagemodel.h"
#include "lattice_p.h"
#include <boost/functional/hash.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace libime {

struct NBestNode {
    NBestNode(const LatticeNode *node) : node_(node) {}

    const LatticeNode *node_;
    // for nbest
    float gn_ = 0.0f;
    float fn_ = -std::numeric_limits<float>::max();
    NBestNode *next_ = nullptr;
};

class DecoderPrivate {
public:
    DecoderPrivate(Decoder *q, Dictionary *dict, LanguageModelBase *model)
        : q_ptr(q), dict_(dict), model_(model) {}

    void buildLattice(
        Lattice &l, const std::unordered_set<const SegmentGraphNode *> &ignore,
        const State &state, const SegmentGraph &graph, size_t frameSize) const {
        FCITX_Q();
        LatticeMap &lattice = l.d_ptr->lattice_;

        if (!lattice.count(&graph.start())) {
            lattice[&graph.start()].push_back(
                q->createLatticeNode(graph, model_, "", model_->beginSentence(),
                                     {nullptr, &graph.start()}, state, 0));
        }

        std::unordered_map<
            std::pair<const SegmentGraphNode *, const SegmentGraphNode *>,
            size_t, boost::hash<std::pair<const SegmentGraphNode *,
                                          const SegmentGraphNode *>>>
            dupPath;
        dict_->matchPrefix(
            graph,
            [this, &graph, &lattice, &dupPath,
             frameSize](const SegmentGraphPath &path, boost::string_view entry,
                        float adjust, boost::string_view aux) {
                FCITX_Q();
                WordIndex idx = model_->index(entry);
                assert(path.front());
                size_t &dupSize =
                    dupPath[std::make_pair(path.front(), path.back())];
                if (dupSize > frameSize && path.front() != &graph.start()) {
                    return;
                }
                auto node = q->createLatticeNode(graph, model_, entry, idx,
                                                 path, model_->nullState(),
                                                 adjust, aux, dupSize);
                if (node) {
                    lattice[path.back()].push_back(node);
                    dupSize++;
                }
            },
            ignore);
        assert(lattice.count(&graph.end()));
        lattice[nullptr].push_back(
            q->createLatticeNode(graph, model_, "", model_->endSentence(),
                                 {&graph.end(), nullptr}, model_->nullState()));
    }

    Decoder *q_ptr;
    FCITX_DECLARE_PUBLIC(Decoder);
    Dictionary *dict_;
    LanguageModelBase *model_;
};

Decoder::Decoder(Dictionary *dict, LanguageModelBase *model)
    : d_ptr(std::make_unique<DecoderPrivate>(this, dict, model)) {}

Decoder::~Decoder() {}

std::string concatNBest(NBestNode *node, boost::string_view sep = "") {
    std::stringstream ss;
    while (node != nullptr) {
        ss << node->node_->word() << sep;
        node = node->next_;
    }
    return ss.str();
}

const LanguageModelBase *Decoder::model() const {
    FCITX_D();
    return d->model_;
}

// #define DEBUG_TIME

void Decoder::decode(Lattice &l, const SegmentGraph &graph, size_t nbest,
                     const State &beginState, float max, float min,
                     size_t beamSize, size_t frameSize) const {
    FCITX_D();
    LatticeMap &lattice = l.d_ptr->lattice_;
#ifdef DEBUG_TIME
    auto t0 = std::chrono::high_resolution_clock::now();
#endif

    l.d_ptr->nbests.clear();
    l.d_ptr->lattice_.erase(nullptr);
    std::unordered_set<const SegmentGraphNode *> ignore;
    for (auto &p : lattice) {
        ignore.insert(p.first);
    }

    std::unordered_map<const SegmentGraphNode *,
                       std::tuple<float, LatticeNode *, State>>
        unknownIdCache;

    d->buildLattice(l, ignore, beginState, graph, frameSize);
#ifdef DEBUG_TIME
    {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::high_resolution_clock::now() -
                         t0).count() /
                         1000000
                  << std::endl;
    }
#endif
    // std::cout << "Lattice Size: " << lattice.size() << std::endl;
    // avoid repeated allocation
    State state;

    // forward search
    auto updateForNode = [&](const SegmentGraphNode *graphNode) {
        if (!lattice.count(graphNode)) {
            return;
        }
        if (ignore.count(graphNode)) {
            return;
        }
        auto &latticeNodes = lattice[graphNode];
#if 0
        if (graphNode) {
            std::cout << "Frame idx: " << graphNode->index() << "Lattice nodes size : " << latticeNodes.size() <<
            std::endl;
        }
#endif
        for (auto &node : latticeNodes) {
            auto from = node.from();
            float maxScore = -std::numeric_limits<float>::max();
            LatticeNode *maxNode = nullptr;
            State maxState;
            bool isUnknown = d->model_->isNodeUnknown(node);
            if (isUnknown) {
                auto iter = unknownIdCache.find(from);
                if (iter != unknownIdCache.end()) {
                    std::tie(maxScore, maxNode, maxState) = iter->second;
                }
            }

            if (!maxNode) {
                auto &searchFrom = lattice[from];
                auto searchSize = beamSize;
                if (searchSize) {
                    searchSize = std::min(searchSize, lattice[from].size());
                } else {
                    searchSize = lattice[from].size();
                }
                for (auto &parent :
                     searchFrom | boost::adaptors::sliced(0, searchSize)) {
                    auto score = parent.score() +
                                 d->model_->score(parent.state(), node, state);
                    if (score > maxScore) {
                        maxScore = score;
                        maxNode = &parent;
                        maxState = state;
                    }
                }

                if (isUnknown) {
                    unknownIdCache.emplace(
                        std::piecewise_construct, std::forward_as_tuple(from),
                        std::forward_as_tuple(maxScore, maxNode, maxState));
                }
            }

            assert(maxNode);
            node.setScore(maxScore + node.cost());
            node.setPrev(maxNode);
            node.state() = maxState;
#if 0
            {
                auto pos = &node;
                auto bos = &lattice[&graph.start()][0];
                std::string sentence;
                while (pos != bos) {
                    sentence = pos->word_ + " " + sentence;
                    pos = pos->prev_;
                }
                std::cout << &node << " " << graphNode << " " << sentence << " " << node.score_ << std::endl;
            }
#endif
        }
        latticeNodes.sort([](const LatticeNode &lhs, const LatticeNode &rhs) {
            return lhs.score() > rhs.score();
        });
    };
    for (size_t i = 1; i <= graph.size(); i++) {
        for (auto &graphNode : graph.nodes(i)) {
            updateForNode(&graphNode);
        }
    }
    updateForNode(nullptr);

#ifdef DEBUG_TIME
    {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::high_resolution_clock::now() -
                         t0).count() /
                         1000000
                  << std::endl;
    }
#endif
    // backward search
    if (nbest == 1) {
        assert(lattice[&graph.start()].size() == 1);
        assert(lattice[nullptr].size() == 1);
        auto pos = &lattice[nullptr][0];
        l.d_ptr->nbests.push_back(pos->toSentenceResult());
    } else {
        struct NBestNodeLess {
            bool operator()(const NBestNode *lhs, const NBestNode *rhs) const {
                return lhs->fn_ < rhs->fn_;
            }
        };
        std::priority_queue<NBestNode *, std::vector<NBestNode *>,
                            NBestNodeLess>
            q, result;
        std::unordered_set<std::string> dup;
        std::list<NBestNode> nbestNodePool;

        auto eos = &lattice[nullptr][0];
        auto newNBestNode = [&nbestNodePool](const LatticeNode *node) {
            nbestNodePool.emplace_back(node);
            return &nbestNodePool.back();
        };
        q.push(newNBestNode(eos));
        auto bos = &lattice[&graph.start()][0];
        while (!q.empty()) {
            auto node = q.top();
            q.pop();
            if (bos == node->node_) {
                auto sentence = concatNBest(node);
                if (dup.count(sentence)) {
                    continue;
                }

                if (eos->score() - node->fn_ > max) {
                    break;
                }
                result.push(node);
                if (result.size() >= nbest) {
                    break;
                }
                dup.insert(sentence);
            } else {
                for (auto &from : lattice[node->node_->from()]) {
                    auto score =
                        d->model_->score(from.state(), *node->node_, state) +
                        node->node_->cost();
                    if (&from != bos && score < min)
                        continue;
                    auto parent = newNBestNode(&from);
                    parent->gn_ = score + node->gn_;
                    parent->fn_ = parent->gn_ + parent->node_->score();
                    parent->next_ = node;

                    q.push(parent);
                }
            }
        }

        while (!result.empty()) {
            auto node = result.top();
            result.pop();
            // loop twice to avoid problem
            size_t count = 0;
            // skip bos
            auto pivot = node->next_;
            while (pivot != nullptr) {
                pivot = pivot->next_;
                count++;
            }
            SentenceResult::Sentence result;
            result.reserve(count);
            pivot = node->next_;
            while (pivot != nullptr) {
                if (pivot->node_->to()) {
                    result.emplace_back(pivot->node_);
                }
                pivot = pivot->next_;
            }
            l.d_ptr->nbests.emplace_back(std::move(result), node->fn_);
        }
    }
#ifdef DEBUG_TIME
    {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::high_resolution_clock::now() -
                         t0).count() /
                         1000000
                  << std::endl;
    }
#endif
}

LatticeNode *
Decoder::createLatticeNodeImpl(const SegmentGraph &, LanguageModelBase *,
                               boost::string_view word, WordIndex idx,
                               SegmentGraphPath path, const State &state,
                               float cost, boost::string_view aux, bool) const {
    return new LatticeNode(word, idx, std::move(path), state, cost, aux);
}
}
