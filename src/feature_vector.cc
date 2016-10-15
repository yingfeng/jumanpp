#include "feature_vector.h"

// 差分から新しいFeatureVectorオブジェクトを生成
FeatureVector::FeatureVector(const std::vector<unsigned long> &v1,
                             const std::vector<unsigned long> &v2) { /*{{{*/
    for (const auto &s : v1) {
        vec[s] += 1.0;
    }
    for (const auto &s : v2) {
        vec[s] -= 1.0;
    }
}; /*}}}*/

// (素性のインデックスのみを受け取る)
FeatureVector::FeatureVector(const std::vector<unsigned long> &v1) { /*{{{*/
    for (const auto &s : v1) {
        vec[s] += 1.0;
    }
}; /*}}}*/

// コピーコンストラクタ
FeatureVector::FeatureVector(const FeatureVector &fv) { /*{{{*/
    mmap_flag = false; // コピーした時点で，メモリマップは無効
    if (fv.mmap_flag) {
        for (const auto &p : *fv.dvec)
            vec[p.first] = p.second;
    } else {
        for (const auto &p : fv.vec)
            vec[p.first] = p.second;
    }
}; /*}}}*/

FeatureVector &FeatureVector::operator=(const FeatureVector &fv) { /*{{{*/
    mmap_flag = false; // 代入した時点で，メモリマップは無効
    if (fv.mmap_flag) {
        for (const auto &p : *fv.dvec)
            vec[p.first] = p.second;
    } else {
        for (const auto &p : fv.vec)
            vec[p.first] = p.second;
    }
    return *this;
}; /*}}}*/

double FeatureVector::operator*(const FeatureVector &fv) const { /*{{{*/
    double sum = 0.0;
    if (mmap_flag) {
        if (this->size() > fv.size()) {
            for (const auto &f : fv) {
                auto itr = dvec->find(f.first);
                if (itr != dvec->end())
                    sum += f.second * itr->second;
            }
        } else {
            for (const auto &f : (*dvec)) {
                auto itr = fv.vec.find(f.first);
                if (itr != fv.vec.end())
                    sum += f.second * itr->second;
            }
        }
    } else {
        if (this->size() > fv.size()) {
            for (const auto &f : fv) {
                auto itr = vec.find(f.first);
                if (itr != vec.end())
                    sum += f.second * itr->second;
            }
        } else {
            for (const auto &f : vec) {
                auto itr = fv.find(f.first);
                if (itr != fv.end())
                    sum += f.second * itr->second;
            }
        }
    }
    return sum;
}; /*}}}*/

FeatureVector::iterator FeatureVector::FeatureVector::begin() {
    return FeatureVectorIterator(this, true);
};
FeatureVector::iterator FeatureVector::FeatureVector::end() {
    return FeatureVectorIterator(this, false);
};
FeatureVector::const_iterator FeatureVector::FeatureVector::begin() const {
    return ConstFeatureVectorIterator(this, true);
};
FeatureVector::const_iterator FeatureVector::FeatureVector::end() const {
    return ConstFeatureVectorIterator(this, false);
};

FeatureVector &FeatureVector::merge(const FeatureVector &fv) { /*{{{*/
    if (mmap_flag) {
        for (const auto &st : fv) {
            (*dvec)[st.first] += st.second;
        }
    } else {
        for (const auto &st : fv) {
            vec[st.first] += st.second;
        }
    }
    return *this;
} /*}}}*/

FeatureVector &FeatureVector::diff(const FeatureVector &fv) { /*{{{*/
    if (mmap_flag) {
        for (const auto &st : fv) {
            (*dvec)[st.first] -= st.second;
        }
    } else {
        for (const auto &st : fv) {
            vec[st.first] -= st.second;
        }
    }
    return *this;
} /*}}}*/

FeatureVector::iterator FeatureVector::find(const unsigned long key) { /*{{{*/
    if (mmap_flag) {
        auto ditr = dvec->find(key);
        return FeatureVectorIterator(this, ditr);
    } else {
        auto itr = vec.find(key);
        return FeatureVectorIterator(this, itr);
    }
}; /*}}}*/

FeatureVector::const_iterator
FeatureVector::find(const unsigned long key) const { /*{{{*/
    if (mmap_flag) {
        auto ditr = dvec->find(key);
        return ConstFeatureVectorIterator(this, ditr);
    } else {
        auto itr = vec.find(key);
        return ConstFeatureVectorIterator(this, itr);
    }
}; /*}}}*/
