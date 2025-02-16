#ifndef PENGUIN_RESULT_HPP_
#define PENGUIN_RESULT_HPP_

#include <iomanip>
#include <limits>
#include <queue>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "../3rdparty/json/include/json.hpp"
#include "../3rdparty/md5/include/md5.hpp"
#include "core.hpp"
#include "recognize.hpp"

#include "act24.hpp"

using dict = nlohmann::ordered_json;
// extern void show_img(cv::Mat src);

namespace penguin
{ // result
const int BASELINE_V_HEIGHT_MIN = 10;
const int RESULT_DIST_THRESHOLD = 25;
const double STAR_WIDTH_PROP = 0.4;
const double DROP_AREA_X_PROP = 0.21;
const double DROP_AREA_Y_PROP = 0.2;
const double DROP_AREA_HEIGHT_PROP = 0.8;
const double ITEM_DIAMETER_PROP = 0.524;
const double ACT24_DIAMETER_PROP = 0.355;
const double W_H_PROP = 7;

enum DroptypeFlags
{
    UNDEFINED = 0,
    SANITY = 1,
    FIRST = 2,
    LMB = 3,
    FURNITURE = 4,
    NORMAL_DROP = 5,
    SPECIAL_DROP = 6,
    EXTRA_DROP = 7
};

enum HsvFlags
{
    H = 0,
    S = 1,
    V = 2
};

std::map<DroptypeFlags, std::string> Droptype2Str {
    {DroptypeFlags::UNDEFINED, ""},
    {DroptypeFlags::SANITY, "SANITY"},
    {DroptypeFlags::FIRST, "FIRST"},
    {DroptypeFlags::LMB, "LMB"},
    {DroptypeFlags::FURNITURE, "FURNITURE"},
    {DroptypeFlags::NORMAL_DROP, "NORMAL_DROP"},
    {DroptypeFlags::SPECIAL_DROP, "SPECIAL_DROP"},
    {DroptypeFlags::EXTRA_DROP, "EXTRA_DROP"}};

const std::map<int, DroptypeFlags> HSV_H2Droptype {
    {51, DroptypeFlags::LMB},
    {201, DroptypeFlags::FIRST},
    {0, DroptypeFlags::NORMAL_DROP},
    {360, DroptypeFlags::NORMAL_DROP},
    {25, DroptypeFlags::SPECIAL_DROP},
    {63, DroptypeFlags::EXTRA_DROP},
    {24, DroptypeFlags::FURNITURE}};

class Widget_ResultLabel : public Widget
{
public:
    const bool is_result() const { return _is_result; }
    Widget_ResultLabel() = default;
    Widget_ResultLabel(Widget* const parent_widget)
        : Widget("result", parent_widget) {}
    Widget_ResultLabel(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : Widget("result", parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        if (!img.empty() && img.channels() == 3)
        {
            cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
            cv::threshold(_img_bin, _img_bin, 200, 255, cv::THRESH_BINARY);
            cv::Rect img_rect = cv::boundingRect(_img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = _img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    void set_img(cv::Mat img, cv::Mat img_bin)
    {
        if (!img.empty() && img.channels() == 3 &&
            !img_bin.empty() && img_bin.channels() == 1)
        {
            cv::Rect img_rect = cv::boundingRect(img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    Widget_ResultLabel& analyze()
    {
        if (!_img.empty())
        {
            _get_is_result();
        }
        if (!_is_result)
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_FALSE);
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));
        if (!debug)
        {
            rpt["isResult"] = _is_result;
        }
        else
        {
            rpt["isResult"] = _is_result;
            rpt["hash"] = _hash;
            rpt["dist"] = _dist;
        }
        return rpt;
    }

private:
    bool _is_result = false;
    std::string _hash = "";
    int _dist = int(HammingFlags::HAMMING64) * 4;
    void _get_is_result()
    {
        auto result_img_bin = _img_bin;
        _hash = shash(result_img_bin, ResizeFlags::RESIZE_W32_H8);
        std::string hash_std =
            resource.get<dict>("hash_index")["result"][server];
        _dist = hamming(_hash, hash_std);
        if (_dist <= RESULT_DIST_THRESHOLD)
        {
            _is_result = true;
        }
    }
};

class Widget_Stars : public Widget
{
public:
    using Widget::set_img;
    const bool is_3stars() const { return _stars == 3; }
    Widget_Stars() = default;
    Widget_Stars(Widget* const parent_widget)
        : Widget("stars", parent_widget) {}
    Widget_Stars(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : Widget("stars", parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        if (!img.empty() && img.channels() == 3)
        {
            _img = img;
            cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
            cv::threshold(_img_bin, _img_bin, 127, 255, cv::THRESH_BINARY);
            _get_abs_pos();
        }
    }
    Widget_Stars& analyze()
    {
        if (!_img.empty())
        {
            _get_is_3stars();
        }
        if (_stars != 3)
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_FALSE);
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));
        if (!debug)
        {
            rpt["count"] = _stars;
        }
        else
        {
            rpt["count"] = _stars;
        }
        return rpt;
    }

private:
    int _stars = 0;
    void _get_is_3stars()
    {
        cv::Mat star_img_bin = _img_bin;
        auto sp_ = separate(star_img_bin, DirectionFlags::RIGHT, 1);
        cv::Range laststar_range = separate(star_img_bin, DirectionFlags::RIGHT, 1)[0];
        cv::Mat laststar = star_img_bin(cv::Range(0, height), laststar_range);
        cv::Rect star_rect = cv::boundingRect(laststar);
        if (star_rect.empty())
        {
            return;
        }
        auto sp = separate(star_img_bin(cv::Rect(0, 0, width, height / 2)), DirectionFlags::LEFT);
        for (auto it = sp.cbegin(); it != sp.cend();)
        {
            const auto& range = *it;
            if (const auto length = range.end - range.start;
                length < height * STAR_WIDTH_PROP)
            {
                it = sp.erase(it);
            }
            else
            {
                ++it;
            }
        }
        star_rect.x = sp.front().start;
        star_rect.width = sp.back().end - star_rect.x;
        _img = _img(star_rect);
        _get_abs_pos();
        _stars = static_cast<int>(sp.size());
    }
};

class Widget_Stage : public WidgetWithCandidate<std::vector<int>, int>
{
public:
    const std::string stage_code() const { return _stage_code(); }
    const std::string stageId() const { return _stageId(); }
    Widget_Stage() = default;
    Widget_Stage(Widget* const parent_widget)
        : WidgetWithCandidate("stage", parent_widget) {}
    Widget_Stage(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : WidgetWithCandidate("stage", parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        if (!img.empty() && img.channels() == 3)
        {
            cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
            double maxval;
            cv::minMaxIdx(_img_bin, nullptr, &maxval);
            cv::threshold(_img_bin, _img_bin, maxval * 0.9, 255, cv::THRESH_BINARY);
            cv::Rect img_rect = cv::boundingRect(_img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = _img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    void set_img(cv::Mat img, cv::Mat img_bin)
    {
        if (!img.empty() && img.channels() == 3 &&
            !img_bin.empty() && img_bin.channels() == 1)
        {
            cv::Rect img_rect = cv::boundingRect(img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    Widget_Stage& analyze()
    {
        if (!_img.empty())
        {
            _get_stage();
        }
        if (_stageId().empty())
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_NOTFOUND);
        }
        else if (const auto& stage_index = resource.get<dict>("stage_index");
                 stage_index[_stage_code()][_difficulty]["existence"] == false)
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_ILLEGAL);
        }
        return *this;
    }
    bool _next_candidate()
    {
        return _set_candidate(_candidate_index + 1);
    }
    bool _set_candidate(int index)
    {
        if (index < _candidates.size())
        {
            size_t length = _stage_chrs.size();
            auto candidate = _candidates[index];
            for (size_t i = 0; i < length; ++i)
            {
                if (_stage_chrs[i].candidate_index() != candidate.key[i])
                {
                    _stage_chrs[i]._set_candidate(candidate.key[i]);
                }
            }
            _candidate_index = index;
            return true;
        }
        return false;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));
        if (!debug)
        {
            rpt["stageCode"] = _stage_code();
            rpt["stageId"] = _stageId();
        }
        else
        {
            rpt["stageCode"] = _stage_code();
            rpt["stageId"] = _stageId();
            for (int i = 0; i < _candidates.size(); ++i)
            {
                rpt["dist"][_stage_code(i)] = _measure(i);
            }
            for (auto& chr : _stage_chrs)
            {
                rpt["chars"].push_back(chr.report(debug));
            }
        }
        return rpt;
    }
    // for temporary workaround
    void _set_difficulty(const std::string& difficulty)
    {
        _difficulty = difficulty;
    }

private:
    const int _CANDIDATES_COUNT = 5;
    bool _existance = false;
    std::string _difficulty = "NORMAL"; // for temporary workaround
    std::vector<Widget_Character> _stage_chrs;
    const std::string _stage_code() const
    {
        return _stage_code(_candidate_index);
    }
    const std::string _stage_code(const int index) const
    {
        std::string code;
        size_t length = _stage_chrs.size();
        for (size_t i = 0; i < length; ++i)
        {
            int index_i = _key(index)[i];
            code += _stage_chrs[i].candidates()[index_i].key;
        }
        return code;
    }
    const std::string _stageId() const
    {
        auto stageId = _stageId(_candidate_index);
        return stageId;
    }
    const std::string _stageId(const int index) const
    {
        auto stage_code = _stage_code(index);
        if (const auto& stage_index = resource.get<dict>("stage_index");
            stage_index.contains(stage_code))
        {
            return (std::string)stage_index[stage_code][_difficulty]["stageId"];
        }
        else
        {
            return "";
        }
    }
    void _get_stage()
    {
        cv::Mat stage_img = _img;
        cv::Mat stage_img_bin = _img_bin;
        auto sp = separate(stage_img_bin, DirectionFlags::LEFT);
        for (const auto& range : sp)
        {
            int length = range.end - range.start;
            cv::Rect char_rect = cv::Rect(range.start, 0, length, height);
            cv::Mat char_img = stage_img(char_rect);
            cv::Mat char_img_bin = stage_img_bin(char_rect);
            std::string label = "char." + std::to_string(_stage_chrs.size());
            auto font = FontFlags::NOVECENTO_WIDEMEDIUM;
            Widget_Character chr {font, label, this};
            chr.set_img(char_img, char_img_bin);
            chr.analyze();
            _stage_chrs.emplace_back(chr);
        }

        std::vector<int> key;
        int measure = 0;
        for (const auto& chr : _stage_chrs)
        {
            key.emplace_back(chr.candidate_index());
            measure += chr.dist();
        }
        _candidates.emplace_back(key, measure);

        if (_stageId().empty())
        {
            _get_candidates();
        }
        while (_stageId().empty())
        {
            if (!_next_candidate())
            {
                break;
            }
        }
    }
    void _get_candidates()
    {
        auto comp = [](std::vector<Widget_Character> a,
                       std::vector<Widget_Character> b) {
            int dist_a = 0, dist_b = 0;
            for (const auto& chr : a)
            {
                dist_a += chr.dist();
            }
            for (const auto& chr : b)
            {
                dist_b += chr.dist();
            }
            return dist_a > dist_b;
        };
        std::priority_queue<
            std::vector<Widget_Character>,
            std::vector<std::vector<Widget_Character>>,
            decltype(comp)>
            q(comp);
        auto last_pop = _stage_chrs;
        size_t length = _stage_chrs.size();
        while (_candidates.size() <= _CANDIDATES_COUNT)
        {
            for (size_t i = 0; i < length; ++i)
            {
                auto child = last_pop;
                child[i]._next_candidate();
                q.push(child);
            }
            last_pop = q.top();
            q.pop();
            std::vector<int> key;
            int measure = 0;
            for (const auto& chr : last_pop)
            {
                key.emplace_back(chr.candidate_index());
                measure += chr.dist();
            }
            _candidates.emplace_back(key, measure);
        }
    }
};

class Widget_Difficulty : public Widget
{
public:
    const std::string difficulty() const { return _difficulty; }
    Widget_Difficulty() = default;
    Widget_Difficulty(Widget* const parent_widget)
        : Widget("difficulty", parent_widget) {}
    Widget_Difficulty(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : Widget("difficulty", parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        if (!img.empty() && img.channels() == 3)
        {
            cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
            cv::threshold(_img_bin, _img_bin, 64, 255, cv::THRESH_BINARY);
            cv::Rect img_rect = cv::boundingRect(_img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = _img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    void set_img(cv::Mat img, cv::Mat img_bin)
    {
        if (!img.empty() && img.channels() == 3 &&
            !img_bin.empty() && img_bin.channels() == 1)
        {
            cv::Rect img_rect = cv::boundingRect(img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }

    Widget_Difficulty& analyze()
    {
        if (!_img.empty())
        {
            _get_difficulty();
        }
        if (_difficulty.empty())
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_ILLEGAL);
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));
        if (!debug)
        {
            rpt["difficulty"] = _difficulty;
        }
        else
        {
            rpt["difficulty"] = _difficulty;
        }
        return rpt;
    }

private:
    std::string _difficulty = "NORMAL";
    void _get_difficulty()
    {
        size_t diff_count = 0;

        if (!(height < width / 6))
        {
            cv::Mat diff_img_bin200;
            cv::cvtColor(_img, diff_img_bin200, cv::COLOR_BGR2GRAY);
            cv::threshold(diff_img_bin200, diff_img_bin200, 200, 255, cv::THRESH_BINARY);
            int inspction_line = static_cast<int>(0.5 * cv::boundingRect(diff_img_bin200).y);
            int offset = diff_img_bin200.rows;

            cv::Mat diff_img = _img_bin(cv::Rect(0, inspction_line, width, 1));
            diff_count = separate(diff_img, DirectionFlags::LEFT).size();

            _img.adjustROI(static_cast<int>(1.2 * offset), static_cast<int>(1.2 * offset), 0, 0);
            _get_abs_pos();
        }

        switch (diff_count)
        {
        case 0:
        case 2:
            _difficulty = "NORMAL";
            break;
        case 3:
            _difficulty = "TOUGH";
            break;
        default:
            break;
        }
    }
};

class Widget_DroptypeLine : public WidgetWithCandidate<DroptypeFlags, int>
{
public:
    const DroptypeFlags droptype() const { return _key(); }
    const int dist() const { return _measure(); }
    Widget_DroptypeLine() = default;
    Widget_DroptypeLine(Widget* const parent_widget)
        : WidgetWithCandidate(parent_widget) {}
    Widget_DroptypeLine(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : WidgetWithCandidate(img, parent_widget) {}
    Widget_DroptypeLine& analyze()
    {
        if (!_img.empty())
        {
            _get_candidates();
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        if (!debug)
        {
        }
        else
        {
            rpt["hsv"] = {(int)round(_hsv[H]), _hsv[S], _hsv[V]};
            for (const auto& candidate : _candidates)
            {
                auto type = Droptype2Str[candidate.key];
                rpt["dist"][type] = candidate.measure;
            }
        }
        return rpt;
    }

private:
    cv::Vec3f _hsv;
    void _get_candidates()
    {
        _get_hsv();
        auto hsv = _hsv;
        if (hsv[S] < 0.1 && hsv[V] <= 0.9)
        {
            _candidates.emplace_back(DroptypeFlags::NORMAL_DROP, 0);
            _candidates.emplace_back(DroptypeFlags::SANITY, 180);
            return;
        }
        else if (hsv[S] < 0.1 && hsv[V] > 0.9)
        {
            _candidates.emplace_back(DroptypeFlags::SANITY, 0);
            _candidates.emplace_back(DroptypeFlags::NORMAL_DROP, 180);
            return;
        }
        else
        {
            for (const auto& [kh, vtype] : HSV_H2Droptype)
            {
                if (vtype == DroptypeFlags::NORMAL_DROP || vtype == DroptypeFlags::SANITY)
                {
                    continue;
                }
                int dist = static_cast<int>(round(abs(kh - hsv[H])));
                _candidates.emplace_back(vtype, dist);
            }
            std::sort(_candidates.begin(), _candidates.end(),
                      [](const Candidate& val1, const Candidate& val2) {
                          return val1.measure < val2.measure;
                      });
            _candidates = std::vector<Candidate>(_candidates.cbegin(), _candidates.cbegin() + 3);
        }
    }
    void _get_hsv()
    {
        auto droplineimg = _img;
        cv::Mat4f bgra(cv::Vec4f(cv::mean(droplineimg)));
        cv::Mat3f hsv;
        cv::cvtColor(bgra, hsv, cv::COLOR_BGRA2BGR);
        cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV);
        _hsv = hsv[0][0];
        _hsv[V] /= 255;
    }
};

class Widget_DroptypeText : public WidgetWithCandidate<DroptypeFlags, int>
{
    struct DroptypeDist
    {
        std::string droptype;
        int dist;
        DroptypeDist(std::string droptype_, int dist_)
            : droptype(droptype_), dist(dist_) {}
    };

public:
    const DroptypeFlags droptype() const { return _key(); }
    const int dist() const { return _measure(); }
    Widget_DroptypeText() = default;
    Widget_DroptypeText(Widget* const parent_widget)
        : WidgetWithCandidate(parent_widget) {}
    Widget_DroptypeText(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : WidgetWithCandidate(parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        if (!img.empty() && img.channels() == 3)
        {
            cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
            double maxval;
            cv::minMaxIdx(img, nullptr, &maxval);
            cv::threshold(_img_bin, _img_bin, maxval / 2, 255, cv::THRESH_BINARY);
            while (_img_bin.rows > 0)
            {
                cv::Mat topline = _img_bin.row(0);
                int meanval = static_cast<int>(cv::mean(topline)[0]);
                if (meanval < 127)
                {
                    break;
                }
                else
                {
                    _img_bin.adjustROI(-1, 0, 0, 0);
                }
            }
            cv::Rect img_rect = cv::boundingRect(_img_bin);
            if (!img_rect.empty())
            {
                _img = img(img_rect);
                _img_bin = _img_bin(img_rect);
                _get_abs_pos();
            }
        }
    }
    Widget_DroptypeText& analyze()
    {
        if (!_img.empty())
        {
            _get_candidates();
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        if (!debug)
        {
        }
        else
        {
            rpt["hash"] = _hash;
            for (const auto& candidate : _candidates)
            {
                rpt["dist"][candidate.key] = candidate.measure;
            }
        }
        return rpt;
    }

private:
    std::string _hash = "";
    void _get_candidates()
    {
        cv::Mat droptext_img = _img_bin;
        if (server == "CN" || server == "KR")
        {
            droptext_img.adjustROI(0, 0, 0, -width / 2);
        }
        if (server == "US")
        {
            _hash = shash(droptext_img, ResizeFlags::RESIZE_W32_H8);
        }
        else
        {
            _hash = shash(droptext_img);
        }
        const auto& droptype_dict =
            resource.get<dict>("hash_index")["dropType"][server];
        int dist_spe = hamming(_hash, droptype_dict["SPECIAL_DROP"]);
        _candidates.emplace_back(DroptypeFlags::SPECIAL_DROP, dist_spe);
        int dist_fur = hamming(_hash, droptype_dict["FURNITURE"]);
        _candidates.emplace_back(DroptypeFlags::FURNITURE, dist_fur);
        std::sort(_candidates.begin(), _candidates.end(),
                  [](const Candidate& val1, const Candidate& val2) {
                      return val1.measure < val2.measure;
                  });
    }
};

class Widget_Droptype : public WidgetWithCandidate<DroptypeFlags, int>
{
public:
    const DroptypeFlags droptype() const { return _key(); }
    const int dist() const { return _measure(); }
    const int items_count() const { return _items_count; }
    Widget_Droptype() = default;
    Widget_Droptype(const cv::Mat& img, const std::string& label, Widget* const parent_widget = nullptr)
        : WidgetWithCandidate(label, parent_widget)
    {
        set_img(img);
    }
    void set_img(cv::Mat img)
    {
        cv::cvtColor(img, _img_bin, cv::COLOR_BGR2GRAY);
        double maxval;
        cv::minMaxIdx(_img_bin, nullptr, &maxval);
        cv::threshold(_img_bin, _img_bin, maxval / 2, 255, cv::THRESH_BINARY);
        auto sp = separate(_img_bin, DirectionFlags::BOTTOM, 1);
        if (!sp.empty())
        {
            cv::Rect img_rect = cv::Rect(0, 0, img.cols, sp[0].end);
            _img = img(img_rect);
            _img_bin = _img_bin(img_rect);
            _get_abs_pos();
        }
    }
    Widget_Droptype& analyze()
    {
        if (!_img.empty())
        {
            _get_items_count();
            _get_candidates();
        }
        if (const auto type = droptype();
            type == DroptypeFlags::UNDEFINED ||
            type == DroptypeFlags::SANITY ||
            type == DroptypeFlags::FIRST)
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_ILLEGAL);
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));
        if (!debug)
        {
            rpt["dropType"] = Droptype2Str[droptype()];
            rpt["itemCount"] = _items_count;
        }
        else
        {
            rpt["dropType"] = Droptype2Str[droptype()];
            rpt["itemCount"] = _items_count;
            rpt.merge_patch(_line.report(debug));
            if (!_text.empty())
            {
                rpt.merge_patch(_text.report(debug));
            }
        }
        return rpt;
    }

private:
    int _items_count = 0;
    Widget_DroptypeLine _line {this};
    Widget_DroptypeText _text {this};
    void _get_items_count()
    {
        _items_count = static_cast<int>(round(width / (height * W_H_PROP)));
    }
    void _get_candidates()
    {
        cv::Mat line_img = _img.row(0);
        _line.set_img(line_img);
        _line.analyze();
        _candidates = _line.candidates();
        if (const auto type = droptype();
            type == DroptypeFlags::SPECIAL_DROP ||
            type == DroptypeFlags::FURNITURE)
        {
            auto textimg = _img(cv::Rect(0, 1, width, height - 1));
            _text.set_img(textimg);
            _text.analyze();
            _candidates = _text.candidates();
        }
    }
};

class Widget_DropArea : public Widget
{
    struct Drop
    {
        Widget_Item dropitem;
        DroptypeFlags droptype;
        Drop(const Widget_Item& dropitem_, const DroptypeFlags& droptype_)
            : dropitem(dropitem_), droptype(droptype_) {}
    };

public:
    Widget_DropArea() = default;
    Widget_DropArea(Widget* const parent_widget)
        : Widget("dropArea", parent_widget) {}
    Widget_DropArea(const cv::Mat& img, Widget* const parent_widget = nullptr)
        : Widget(img, "dropArea", parent_widget) {}
    void set_item_diameter(int diameter)
    {
        _item_diameter = diameter;
    }
    Widget_DropArea& analyze(const std::string& stage)
    {
        if (!_img.empty())
        {
            _get_droptypes();
            for (int i = 0; i < 2; i++)
            {
                if (_droptype_validation())
                {
                    break;
                }
                else
                {
                    _next_droptype_candidate();
                }
            }
            if (!_droptype_validation())
            {
                widget_label = "droptypes";
                push_exception(ERROR, ExcSubtypeFlags::EXC_ILLEGAL);
            }
            _get_drops(stage);
        }
        else
        {
            // add img empty exc
        }
        if (_droptype_list.empty())
        {
            push_exception(ERROR, ExcSubtypeFlags::EXC_NOTFOUND, "No droptype found");
        }
        return *this;
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        rpt.merge_patch(Widget::report(debug));

        rpt["dropTypes"] = dict::array();
        rpt["drops"] = dict::array();

        size_t droptypes_count = _droptype_list.size(); // will move in "for" in C++20
        for (int i = 0; i < droptypes_count; i++)
        {
            rpt["dropTypes"].push_back(_droptype_list[i].report(debug));
        }
        size_t drops_count = _drop_list.size(); // will move in "for" in C++20
        for (int i = 0; i < drops_count; i++)
        {
            rpt["drops"].push_back({{"dropType", Droptype2Str[_drop_list[i].droptype]}});
            rpt["drops"][i].merge_patch(_drop_list[i].dropitem.report(debug));
        }
        return rpt;
    }

private:
    int _item_diameter = 0;
    dict _drops_data;
    std::vector<Drop> _drop_list;
    std::vector<Widget_Droptype> _droptype_list;
    auto _get_separate()
    {
        cv::Mat img_bin = _img;
        int offset = static_cast<int>(round(height * 0.75));
        img_bin.adjustROI(-offset, 0, 0, 0);
        cv::cvtColor(img_bin, img_bin, cv::COLOR_BGR2GRAY);
        cv::adaptiveThreshold(img_bin, img_bin, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                              cv::THRESH_BINARY, img_bin.rows / 2 * 2 + 1, -20);

        // get baseline_h
        int row = 0;
        int maxcount = 0; // will move in "for" in C++20
        for (int ro = img_bin.rows - 1; ro >= 0; ro--)
        {
            uchar* pix = img_bin.data + ro * img_bin.step;
            int count = 0; // will move in "for" in C++20
            for (int co = 0; co < img_bin.cols; co++)
            {
                if ((bool)*pix)
                {
                    count++;
                }
                pix++;
            }
            if (count > maxcount)
            {
                row = ro;
                maxcount = count;
            }
        }
        int baseline_h = row + offset;
        auto sp = separate(img_bin(cv::Rect(0, row, width, 1)), DirectionFlags::LEFT);
        for (auto it = sp.cbegin(); it != sp.cend();)
        {
            const auto& range = *it;
            if (const auto length = range.end - range.start;
                length < _item_diameter)
            {
                it = sp.erase(it);
            }
            else
            {
                ++it;
            }
        }
        sp.front().start = 0;
        return std::tuple(baseline_h, sp);
    }
    void _get_droptypes()
    {
        auto [baseline_h, sp] = _get_separate();
        int offset = sp.back().end - width;
        _img.adjustROI(0, 0, 0, offset);
        for (const auto& droptype_range : sp)
        {
            auto droptype_img = _img(cv::Range(baseline_h, height), droptype_range);
            std::string label = "dropTypes." + std::to_string(_droptype_list.size());
            Widget_Droptype droptype {droptype_img, label, this};
            droptype.analyze();
            _droptype_list.emplace_back(droptype);
        }
    }
    bool _droptype_validation()
    {
        int last_type = 0;
        for (const auto& droptype : _droptype_list)
        {
            int current_type = droptype.droptype();
            if (current_type <= last_type)
            {
                return false;
            }
            last_type = current_type;
        }
        return true;
    }
    void _next_droptype_candidate()
    {
        auto comp = [](std::vector<Widget_Droptype> a,
                       std::vector<Widget_Droptype> b) {
            int dist_a = 0, dist_b = 0;
            for (const auto& type : a)
            {
                dist_a += type.dist();
            }
            for (const auto& type : b)
            {
                dist_b += type.dist();
            }
            return dist_a > dist_b;
        };
        std::priority_queue<
            std::vector<Widget_Droptype>,
            std::vector<std::vector<Widget_Droptype>>,
            decltype(comp)>
            q(comp);
        auto last_pop = _droptype_list;
        size_t length = _droptype_list.size();
        for (size_t i = 0; i < length; ++i)
        {
            auto child = last_pop;
            child[i]._next_candidate();
            q.push(child);
        }
        _droptype_list = q.top();
    }

    void _get_drops(const std::string& stage)
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        ItemTemplates templs {stage};
        for (const auto& droptype : _droptype_list)
        {
            if (const auto type = droptype.droptype();
                type == DroptypeFlags::UNDEFINED || type == DroptypeFlags::SANITY ||
                type == DroptypeFlags::FIRST)
            {
                break;
            }
            else if (type == DroptypeFlags::LMB)
            {
                continue;
            }
            else if (type == DroptypeFlags::FURNITURE)
            {
                std::string label =
                    "drops." + std::to_string(_drop_list.size());
                _drop_list.emplace_back(
                    Drop(Widget_Item(FURNI_1, label, this), type));
            }
            else if (templs.templ_list().empty())
            {
                widget_label = "drops";
                push_exception(ERROR, ExcSubtypeFlags::EXC_ILLEGAL, "Empty templetes");
                return;
            }
            else
            {
                int items_count = droptype.items_count();
                int length = (droptype.width) / items_count;
                for (int i = 0; i < items_count; i++)
                {
                    std::string label =
                        "drops." + std::to_string(_drop_list.size());
                    auto range =
                        cv::Range(droptype.x - x + length * i,
                                  droptype.x - x + length * (i + 1));
                    auto dropimg = _img(cv::Range(0, droptype.y - y), range);
                    Widget_Item drop {dropimg, _item_diameter, label, this};
                    drop.analyze(templs);
                    _drop_list.emplace_back(drop, type);
                    _drops_data.push_back({{"dropType", Droptype2Str[type]},
                                           {"itemId", drop.itemId()},
                                           {"quantity", drop.quantity()}});
                }
            }
        }
    }
};

class Result_New : public Widget
{
public:
    Result_New() = default;
    Result_New(const cv::Mat& img)
        : Widget(img) {}
    Result_New& analyze()
    {
        _get_baseline_v();
        _drop_area.set_item_diameter(static_cast<int>(round(_baseline_v.height * ITEM_DIAMETER_PROP)));
        _drop_act24.set_item_diameter(static_cast<int>(round(_baseline_v.height * ACT24_DIAMETER_PROP)));
        _get_result_label();
        _get_stars();
        _get_stage();
        _get_difficulty();
        _stage._set_difficulty(_difficutly.difficulty());
        _get_drop_area();
        if (_stage.stageId().find("act24") != std::string::npos)
            _get_act24_area(_drop_area);
        return *this;
    }
    std::string get_md5()
    {
        MD5 md5;
        return md5(_img.data, _img.step * _img.rows);
    }
    const dict report(bool debug = false)
    {
        dict rpt = dict::object();
        if (_parent_widget == nullptr)
        {
            rpt["exceptions"] = _exception_list;
        }
        if (!debug)
        {
            rpt["resultLabel"] = _result_label.report()["isResult"];
            rpt["stage"] = _stage.report();
            rpt["difficulty"] = _difficutly.report()["difficulty"];
            rpt["stars"] = _stars.report()["count"];
            rpt["dropArea"] = _drop_area.report();
            if (auto& r = rpt["dropArea"]["drops"];
                _stage.stageId().find("act24") != std::string::npos)
            {
                auto e = _drop_act24.report()["drops"];
                r.insert(r.end(), e.begin(), e.end());
            }
        }
        else
        {
            rpt["resultLabel"] = _result_label.report(debug);
            rpt["stage"] = _stage.report(debug);
            rpt["difficulty"] = _difficutly.report(debug);
            rpt["stars"] = _stars.report(debug);
            rpt["dropArea"] = _drop_area.report(debug);
            if (auto& r = rpt["dropArea"]["drops"];
                _stage.stageId().find("act24") != std::string::npos)
            {
                auto e = _drop_act24.report(debug);
                r.insert(r.end(), e["drops"].begin(), e["drops"].end());
                rpt["dropAct24"]["rect"] = e["rect"];
                rpt["dropAct24"]["status"] = e["status"];
            }
        }
        return rpt;
    }

private:
    Widget _baseline_v {this};
    Widget_Stars _stars {this};
    Widget_Stage _stage {this};
    Widget_Difficulty _difficutly {this};
    Widget_ResultLabel _result_label {this};
    Widget_DropArea _drop_area {this};
    Widget_DropAct24 _drop_act24 {this};

    void _get_baseline_v()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        cv::Mat img_bin = _img(cv::Rect(0,
                                        static_cast<int>(0.2 * height),
                                        static_cast<int>(0.2 * width),
                                        static_cast<int>(0.4 * height)));
        cv::cvtColor(img_bin, img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(img_bin, img_bin, 100, 255, cv::THRESH_BINARY);

        auto sp = separate(img_bin, DirectionFlags::LEFT);
        cv::Rect baseline_v_rect;
        for (const auto& range : sp)
        {
            cv::Mat img_temp = img_bin(cv::Range(0, img_bin.rows), range);
            auto sp2 = separate(img_temp, DirectionFlags::TOP);
            if (sp2.size() < 2)
            {
                break;
            }
            int first_height = sp2.front().end - sp2.front().start;
            int last_height = sp2.back().end - sp2.back().start;
            if (abs(img_temp.cols - first_height) <= (img_temp.cols + first_height) * 0.1 &&
                abs(img_temp.cols - last_height) <= (img_temp.cols + last_height) * 0.1 &&
                abs(first_height - last_height) <= (first_height + last_height) * 0.1)
            {
                baseline_v_rect =
                    cv::Rect(range.start, sp2.front().start + static_cast<int>(0.2 * height),
                             img_temp.cols, sp2.back().end - sp2.front().start);
                break;
            }
        }

        cv::Mat baseline_v_img;
        if (!baseline_v_rect.empty())
        {
            baseline_v_img = _img(baseline_v_rect);
        }
        _baseline_v.set_img(baseline_v_img);

        if (_baseline_v.empty())
        {
            _get_baseline_v_fallback();
        }
    }
    void _get_baseline_v_fallback()
    {
        struct ConnectedComponent
        {
            int x, y, width, height, area;
            ConnectedComponent(int x_, int y_, int width_, int height_, int area_)
                : x(x_), y(y_), width(width_), height(height_), area(area_) {}
        };

        cv::Mat img_bin = _img(cv::Rect(0,
                                        static_cast<int>(0.2 * height),
                                        static_cast<int>(0.2 * width),
                                        static_cast<int>(0.4 * height)));
        cv::cvtColor(img_bin, img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(img_bin, img_bin, 120, 255, cv::THRESH_BINARY);
        cv::Mat _;
        cv::Mat1i stats;
        cv::Mat1d centroids;
        int ccomp_count = cv::connectedComponentsWithStats(img_bin, _, stats, centroids);

        std::vector<ConnectedComponent> ccomps;
        for (int i = 1; i < ccomp_count; ++i)
        {
            int ccx = stats(i, cv::CC_STAT_LEFT);
            int ccy = stats(i, cv::CC_STAT_TOP);
            int ccwidth = stats(i, cv::CC_STAT_WIDTH);
            int ccheight = stats(i, cv::CC_STAT_HEIGHT);
            int ccarea = stats(i, cv::CC_STAT_AREA);
            if (ccwidth > 0.01 * height &&
                ccheight > 0.01 * height &&
                (double)ccarea / (ccwidth * ccheight) > 0.95 &&
                abs(ccwidth - ccheight) <= (ccwidth + ccheight) * 0.1)
            {
                ccomps.emplace_back(ccx, ccy, ccwidth, ccheight, ccarea);
            }
        }
        std::sort(ccomps.begin(), ccomps.end(),
                  [](const ConnectedComponent& val1, ConnectedComponent& val2) {
                      if (val1.x == val2.x)
                      {
                          return val1.y < val2.y;
                      }
                      else
                      {
                          return val1.x < val2.x;
                      }
                  });

        cv::Rect baseline_v_rect;
        if (ccomps.size() == 2)
        {
            if (ccomps[0].x == ccomps[1].x)
            {
                baseline_v_rect =
                    cv::Rect(ccomps[0].x,
                             ccomps[0].y + static_cast<int>(0.2 * height),
                             ccomps[0].width,
                             ccomps[1].y - ccomps[0].y + ccomps[1].height);
            }
        }
        else if (const auto ccsize = ccomps.size();
                 ccsize > 2)
        {
            for (int i = 0; i < ccsize - 1; ++i)
            {
                if (ccomps[i].x == ccomps[i + 1].x)
                    baseline_v_rect =
                        cv::Rect(ccomps[i].x,
                                 ccomps[i].y + static_cast<int>(0.2 * height),
                                 ccomps[i].width,
                                 ccomps[i + 1].y - ccomps[i].y + ccomps[i + 1].height);
            }
        }

        cv::Mat baseline_v_img;
        if (!baseline_v_rect.empty())
        {
            baseline_v_img = _img(baseline_v_rect);
        }
        _baseline_v.set_img(baseline_v_img);

        if (_baseline_v.empty())
        {
            _result_label.push_exception(ERROR, ExcSubtypeFlags::EXC_FALSE);
        }
    }
    void _get_result_label()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        const auto& bv = _baseline_v;
        int left_margin = bv.x + bv.width;
        auto result_img =
            _img(cv::Range(bv.y, bv.y + bv.height / static_cast<int>(server == "US" ? 1.5 : 2)),
                 cv::Range(left_margin, left_margin + static_cast<int>(1.5 * bv.height)));
        cv::Mat img_bin;
        cv::cvtColor(result_img, img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(img_bin, img_bin, 200, 255, cv::THRESH_BINARY);
        auto sp = separate(img_bin, DirectionFlags::TOP, (server == "US" ? 2 : 1));
        result_img = result_img(cv::Range(sp.front().start, sp.back().end),
                                cv::Range(0, img_bin.cols));
        _result_label.set_img(result_img);
        _result_label.analyze();
    }
    void _get_stars()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        const auto& bv = _baseline_v;
        int left_margin = bv.x + bv.width;
        auto star_img =
            _img(cv::Range(bv.y + bv.height / 2, bv.y + bv.height),
                 cv::Range(left_margin, left_margin + static_cast<int>(1.2 * bv.height)));
        cv::Mat star_img_bin;
        cv::cvtColor(star_img, star_img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(star_img_bin, star_img_bin, 180, 255, cv::THRESH_BINARY);
        auto sp = separate(star_img_bin, DirectionFlags::BOTTOM, 1);
        if (!sp.empty())
        {
            star_img = star_img(sp[0], cv::Range(0, star_img_bin.cols));
            star_img_bin = star_img_bin(sp[0], cv::Range(0, star_img_bin.cols));
            _stars.set_img(star_img, star_img_bin);
            _stars.analyze();
        }
    }
    void _get_stage()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        const auto& bv = _baseline_v;
        int left_margin = bv.x + bv.width;
        auto stage_img =
            _img(cv::Range(0, bv.y),
                 cv::Range(left_margin + static_cast<int>(0.43 * bv.height),
                           left_margin + static_cast<int>(1.5 * bv.height)));
        cv::Mat stage_img_bin;
        cv::cvtColor(stage_img, stage_img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(stage_img_bin, stage_img_bin, 200, 255, cv::THRESH_BINARY);
        auto sp = separate(stage_img_bin, DirectionFlags::TOP, 1);
        if (!sp.empty())
        {
            stage_img = stage_img(sp[0], cv::Range(0, stage_img_bin.cols));
            stage_img_bin = stage_img_bin(sp[0], cv::Range(0, stage_img_bin.cols));
            _stage.set_img(stage_img, stage_img_bin);
            _stage.analyze();
        }
    }
    void _get_difficulty()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        const auto& bv = _baseline_v;
        int left_margin = bv.x + bv.width;
        auto diff_img =
            _img(cv::Range(0, bv.y),
                 cv::Range(left_margin, left_margin + static_cast<int>(0.43 * bv.height)));
        cv::Mat diff_img_bin;
        cv::cvtColor(diff_img, diff_img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(diff_img_bin, diff_img_bin, 64, 255, cv::THRESH_BINARY);
        auto sp = separate(diff_img_bin, DirectionFlags::TOP, 1);
        if (!sp.empty())
        {
            diff_img = diff_img(sp[0], cv::Range(0, diff_img_bin.cols));
            diff_img_bin = diff_img_bin(sp[0], cv::Range(0, diff_img_bin.cols));
            _difficutly.set_img(diff_img, diff_img_bin);
            _difficutly.analyze();
        }
    }
    void _get_drop_area()
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        const auto& bv = _baseline_v;
        cv::Mat img_bin =
            _img(cv::Range(bv.y + bv.height, height),
                 cv::Range(bv.x + bv.width, static_cast<int>(0.2 * width)));
        cv::cvtColor(img_bin, img_bin, cv::COLOR_BGR2GRAY);
        cv::threshold(img_bin, img_bin, 127, 255, cv::THRESH_BINARY);
        auto sp = separate(img_bin, DirectionFlags::TOP);
        // int top_margin = bv.y + bv.height + sp[1].start;
        int top_line = bv.y + bv.height + sp.front().start;
        int bottom_line = bv.y + bv.height + sp.back().end;
        // int top_margin = bv.y + bv.height + static_cast<int>(height * 0.12);
        auto drop_area_img = _img(
            cv::Range(top_line + static_cast<int>((bottom_line - top_line) * 0.375), bottom_line),
            cv::Range(bv.x + bv.width, width));
        _drop_area.set_img(drop_area_img);
        _drop_area.analyze(_stage.stage_code());
    }
    void _get_act24_area(Widget_DropArea drop_area)
    {
        if (_status == StatusFlags::HAS_ERROR || _status == StatusFlags::ERROR)
        {
            return;
        }
        cv::Mat img_act24 = _img(
            cv::Range(drop_area.y, drop_area.y + drop_area.height),
            cv::Range(drop_area.x + drop_area.width, width));
        _drop_act24.set_img(img_act24);
        _drop_act24.analyze(_stage.stage_code());
    }
};

} // namespace penguin

#endif // PENGUIN_RESULT_HPP_
