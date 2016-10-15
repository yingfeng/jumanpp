#include "tagger.h"
#include <iterator> // std::distance

namespace Morph {

Tagger::Tagger(Parameter *in_param)
    : weight(), scw(in_param->c_value, in_param->phi_value, weight) { //{{{
    param = in_param;
    ftmpl.open(param->ftmpl_filename);
    ftmpl.set_weight = &weight;

    dic.open(param, &ftmpl);

    begin_node_list.reserve(INITIAL_MAX_INPUT_LENGTH);
    end_node_list.reserve(INITIAL_MAX_INPUT_LENGTH);

    sentences_for_train_num = 0;

    // lzma_stream initial_lzma = LZMA_STREAM_INIT;
    // lzma_ = initial_lzma;

} //}}}

Tagger::~Tagger() { //{{{
} //}}}

// analyze a sentence
Sentence *Tagger::new_sentence_analyze(std::string &in_sentence) { //{{{
    begin_node_list.clear();
    end_node_list.clear();
    // TODO: このnew が，メモリ確保&開放の手間がかかるため無駄, clear
    // 程度でなんとかならないか．
    sentence = new Sentence(&begin_node_list, &end_node_list, in_sentence, &dic,
                            &ftmpl, param);
    FeatureSet::topic = nullptr;
    sentence->lookup_and_analyze();
    FeatureSet::topic = nullptr;
    return sentence;
} //}}}

// analyze partially annotated sentence
Sentence *Tagger::partial_annotation_analyze(std::string &in_sentence) { //{{{
    begin_node_list.clear();
    end_node_list.clear();

    // param に user_partial_annotate オプションが渡る
    std::string delimiter = param->delimiter;
    sentence = new Sentence(&begin_node_list, &end_node_list, in_sentence, &dic,
                            &ftmpl, param, delimiter);
    FeatureSet::topic = nullptr;
    sentence->lookup_and_analyze_partial();
    FeatureSet::topic = nullptr;
    return sentence;
} //}}}

// 学習
int Tagger::online_learning(Sentence *gold, Sentence *system,
                            TopicVector *topic = nullptr) { //{{{
    if (param->use_scw) {
        double loss = system->eval(*gold); //単語が異なる割合など
        if (loss > 0.0) {
            if (gold->get_feature()) {
                FeatureVector gold_vec(gold->get_feature()->get_fset());
                FeatureVector sys_vec(system->get_best_feature().get_fset());

                scw.update(loss, gold_vec.diff(sys_vec));
            } else {
                cerr << "update failed" << sentence << endl;
                return 1;
            }
        }
    } else { //パーセプトロン
        std::cerr << "perceptron is not implemented. " << std::endl;
    }
    return 0;
} //}}}

// clear a sentence
void Tagger::sentence_clear() { //{{{
    delete sentence;
    sentence = nullptr;
} //}}}

// train
bool Tagger::train(const std::string &gsd_file) { //{{{
    read_gold_data(gsd_file);

    for (size_t t = 0; t < param->iteration_num; t++) {
        double loss = 0, loss_sum = 0;
        double loss_sum_first_quarter = 0;
        double loss_sum_last_quarter = 0;
        cerr << "ITERATION:" << t << endl;
        if (param->shuffle_training_data) // shuffle training data
            random_shuffle(sentences_for_train.begin(),
                           sentences_for_train.end());

        for (std::vector<Sentence *>::iterator gold =
                 sentences_for_train.begin();
             gold != sentences_for_train.end(); gold++) {
            Sentence *sent =
                new_sentence_analyze((*gold)->get_sentence()); // 通常の解析
            loss = sent->eval(**gold); // 表示用にロスを計算
            loss_sum += loss;

            // 表示用の loss の区間平均
            if (std::distance(sentences_for_train.begin(), gold) <
                std::distance(sentences_for_train.begin(),
                              sentences_for_train.end()) /
                    4) {
                loss_sum_first_quarter += loss;
            } else if (std::distance(sentences_for_train.begin(), gold) >
                       3 * (std::distance(sentences_for_train.begin(),
                                          sentences_for_train.end()) /
                            4)) {
                loss_sum_last_quarter += loss;
            }

            cerr << "\033[2K\r" //行のクリア
                 << std::distance(sentences_for_train.begin(), gold) << "/"
                 << std::distance(sentences_for_train.begin(),
                                  sentences_for_train.end()) //事例数の表示
                 << " avg:"
                 << loss_sum / std::distance(sentences_for_train.begin(), gold)
                 << " loss:"
                 << loss; //イテレーション内の平均ロス，各事例でのロスを表示
            online_learning(*gold, sent);

            // TopicVector sent_topic = sent->get_topic();
            delete (sent);
        }
        cerr << endl;
        cerr << " 1st  quater:" << (loss_sum_first_quarter /
                                    (std::distance(sentences_for_train.begin(),
                                                   sentences_for_train.end()) /
                                     4)) // 最初の1/4 でのロス
             << " last quater:" << (loss_sum_last_quarter /
                                    (std::distance(sentences_for_train.begin(),
                                                   sentences_for_train.end()) /
                                     4)) // 最初の1/4 でのロス
             << " diff: "
             << (loss_sum_first_quarter - loss_sum_last_quarter) /
                    (std::distance(sentences_for_train.begin(),
                                   sentences_for_train.end()) /
                     4)
             << endl;
        write_tmp_model_file(t);
    }
    clear_gold_data();
    return true;
} //}}}

bool Tagger::output_gold_result(const std::string &gsd_file) { //{{{
    read_gold_data(gsd_file);
    clear_gold_data();
    return true;
} //}}}

// train on partially annotated data (仮タイトル)
bool Tagger::ptrain(const std::string &gsd_file) { //{{{
    read_partial_gold_data(gsd_file.c_str());

    for (size_t t = 0; t < param->iteration_num; t++) {
        double loss = 0, loss_sum = 0;
        double loss_sum_first_quarter = 0;
        double loss_sum_last_quarter = 0;
        cerr << "ITERATION:" << t << endl;
        if (param->shuffle_training_data) { // shuffle training data
            random_shuffle(sentences_for_train.begin(),
                           sentences_for_train.end());
            // 部分アノテーションの場合は順番が重要かもしれないので要検討
        }

        for (std::vector<Sentence *>::iterator gold =
                 sentences_for_train.begin();
             gold != sentences_for_train.end(); gold++) {
            // Goldも随時解析しなおす
            Sentence *gold_analysis = partial_annotation_analyze(
                (*gold)->get_pa_sentence()); //ここでdelimiter
            //が除かれていないものが必要
            Sentence *sent_analysis =
                new_sentence_analyze((*gold)->get_sentence()); // 通常の解析

            loss = sent_analysis->eval(*gold_analysis); // 表示用にロスを計算
            loss_sum += loss;

            // 表示用の loss の区間平均
            if (std::distance(sentences_for_train.begin(), gold) <
                std::distance(sentences_for_train.begin(),
                              sentences_for_train.end()) /
                    4) {
                loss_sum_first_quarter += loss;
            } else if (std::distance(sentences_for_train.begin(), gold) >
                       3 * (std::distance(sentences_for_train.begin(),
                                          sentences_for_train.end()) /
                            4)) {
                loss_sum_last_quarter += loss;
            }

            cerr << "\033[2K\r" //行のクリア
                 << std::distance(sentences_for_train.begin(), gold) << "/"
                 << std::distance(sentences_for_train.begin(),
                                  sentences_for_train.end()) //事例数の表示
                 << " avg:"
                 << loss_sum / std::distance(sentences_for_train.begin(), gold)
                 << " loss:"
                 << loss; //イテレーション内の平均ロス，各事例でのロスを表示
            online_learning(gold_analysis, sent_analysis);

            // TopicVector sent_topic = sent->get_topic();
            delete (sent_analysis);
        }
        cerr << endl;
    }

    clear_partial_gold_data();
    return true;
} //}}}

// read gold standard data (wrapper)
bool Tagger::read_gold_data(const std::string &gsd_file) { //{{{
    return read_gold_data(gsd_file.c_str());
} //}}}

// read gold standard data
bool Tagger::read_gold_data(const char *gsd_file) { //{{{
    std::istream *gsd_in;
    std::ifstream gsd_in_fs(gsd_file, std::ios::in);
    if (strlen(gsd_file) >= 1 && gsd_file[0] == '-') {
        gsd_in = &std::cin;
    } else {
        if (!gsd_in_fs.is_open()) {
            cerr << ";; failed to open gold data for reading" << endl;
            return false;
        }
        gsd_in = &gsd_in_fs;
    }

    std::string buffer;
    std::string comment;
    while (getline(*gsd_in, buffer)) {
        std::vector<std::string> word_pos_pairs;
        // comment, id の除去
        size_t pos = buffer.find(" # ");
        if (pos != std::string::npos) {
            // # 以降をコメントとして読み込む
            comment = buffer.substr(pos + 3, std::string::npos);
            // # までをbuffer
            buffer = buffer.substr(0, pos);
        } else {
            comment = "";
        }
        // 単語へ分割
        split_string(buffer, " ", word_pos_pairs);

        Sentence *new_sentence =
            new Sentence(strlen(buffer.c_str()), &begin_node_list,
                         &end_node_list, &dic, &ftmpl, param);
        for (std::vector<std::string>::iterator it = word_pos_pairs.begin();
             it != word_pos_pairs.end(); it++) {
            if (it->size() > 0) {
                new_sentence->lookup_gold_data(*it);
                new_sentence->set_comment(comment);
            }
        }

        new_sentence->find_best_beam();
        new_sentence->set_feature_beam(); // beam のベストの素性を sentence
                                          // にコピーする;
        new_sentence->set_gold_nodes_beam();

        if (param->print_gold)
            new_sentence->print_gold_beam();

        new_sentence->clear_nodes();
        add_one_sentence_for_train(new_sentence);
        sentences_for_train_num++;
    }

    gsd_in_fs.close();
    std::cout << std::endl;
    return true;
} //}}}

// read parcially annotated data
bool Tagger::read_partial_gold_data(const char *gsd_file) { //{{{
    std::ifstream gsd_in(gsd_file, std::ios::in);
    if (!gsd_in.is_open()) {
        cerr << ";; failed to open gold data for reading" << endl;
        return false;
    }

    std::string buffer;
    while (getline(gsd_in, buffer)) {
        //        std::vector<std::string> word_pos_pairs;
        //        split_string(buffer, " ", word_pos_pairs);
        //
        //        Sentence *new_sentence = new Sentence(strlen(buffer.c_str()),
        //        &begin_node_list, &end_node_list, &dic, &ftmpl, param);
        //        for (std::vector<std::string>::iterator it =
        //        word_pos_pairs.begin(); it != word_pos_pairs.end(); it++) {
        //            if(it->size()>0)
        //                new_sentence->lookup_gold_data(*it);
        //        }
        //
        //        if( param->beam ){//beam オプション
        //            new_sentence->find_best_beam(); // tri-gram
        //            素性を抽出するために beam の方を呼ぶ;
        //            new_sentence->set_feature_beam(); // beam のベストの素性を
        //            sentence にコピーする;
        //            new_sentence->set_gold_nodes_beam();
        //        }

        Sentence *new_sentence = partial_annotation_analyze(buffer);
        // new_sentence->print_lattice();
        new_sentence->clear_nodes();
        add_one_sentence_for_train(new_sentence);
        sentences_for_train_num++;
    }

    gsd_in.close();
    std::cout << std::endl;
    return true;
} //}}}

// clear gold standard data
void Tagger::clear_gold_data() { //{{{
    // std::cerr << "clear_gold_data" << std::endl;
    for (std::vector<Sentence *>::iterator it = sentences_for_train.begin();
         it != sentences_for_train.end(); it++) {
        if (*it) {
            delete *it;
            *it = nullptr;
        }
    }
    sentences_for_train.clear();
} //}}}

void Tagger::print_best_beam() { //{{{
    sentence->print_best_beam();
    if (param->debug) {
        sentence->print_unified_lattice();
    }
} //}}}

void Tagger::print_beam() { //{{{
    sentence->print_beam();
    if (param->debug) {
        sentence->print_unified_lattice();
    }
} //}}}

void Tagger::print_best_beam_juman() { //{{{
    sentence->print_best_beam_juman();
} //}}}

void Tagger::print_lattice() { //{{{
    sentence->print_unified_lattice();
} //}}}

void Tagger::print_lattice_rbeam(unsigned int nbest) { //{{{
    sentence->print_unified_lattice_rbeam(nbest);
} //}}}

bool Tagger::add_one_sentence_for_train(Sentence *in_sentence) { //{{{
    sentences_for_train.push_back(in_sentence);
    return true;
} //}}}

//途中経過を書き込む.
bool Tagger::write_tmp_model_file(int t) { //{{{
    std::stringstream ss;
    ss << param->model_filename << "." << t;
    write_bin_model_file(ss.str().c_str());
    return true;
} //}}}
}
