#include "json.hpp"
#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
using namespace std;
using namespace cv;
using dict = nlohmann::json;
#define TOP 0
#define BOTTOM 1
#define LEFT 2
#define RIGHT 3

#define X 0
#define Y 1
#define WIDTH 2
#define HEIGHT 3

#define BEGIN 0
#define END 1

vector<Mat> templs;
dict stage_index, item_index, hash_index;

dict hsv_index = {
    { "51", "LMB" },
    { "201", "FIRST" },
    { "0", "NORMAL_DROP" },
    { "360", "NORMAL_DROP" },
    { "25", "SPECIAL_DROP" },
    { "63", "EXTRA_DROP" },
    { "24", "FURNITURE" }
};

dict droptype_order = {
    { "FIRST", 0 },
    { "LMB", 1 },
    { "FURNITURE", 2 },
    { "NORMAL_DROP", 3 },
    { "SPECIAL_DROP", 4 },
    { "EXTRA_DROP", 5 }
};

vector<int> separate(Mat src_bin, int direc, int n = 0, bool roi = false)
{
    if (src_bin.empty())
        throw invalid_argument("separate(): Image is empty");
    if (src_bin.channels() != 1)
        throw invalid_argument("separate(): Require image in graystyle");
    if (direc < 0 || direc > 3)
        throw invalid_argument("separate(): Invalid direction");

    int offset = 0;
    if (roi) {
        Size wholesize;
        Point upperleft;
        src_bin.locateROI(wholesize, upperleft);
        if (direc >= 2)
            offset = upperleft.x;
        else
            offset = upperleft.y;
    }

    vector<int> sp;
    bool isodd = false;
    if (direc == TOP) {
        for (int ro = 0; ro < src_bin.rows; ro++) {
            uchar* pix = src_bin.data + ro * src_bin.step;
            int co = 0;
            for (; co < src_bin.cols; co++) {
                if ((bool)*pix && !isodd) {
                    sp.push_back(ro + offset);
                    isodd = !isodd;
                    break;
                } else if ((bool)*pix && isodd)
                    break;
                pix++;
            }
            if (co == src_bin.cols && isodd) {
                sp.push_back(ro + offset);
                isodd = !isodd;
                if (sp.size() == 2 * n)
                    break;
            }
        }
        if (isodd)
            sp.push_back(src_bin.rows + offset);
    } else if (direc == BOTTOM) {
        for (int ro = src_bin.rows - 1; ro >= 0; ro--) {
            uchar* pix = src_bin.data + ro * src_bin.step;
            int co = 0;
            for (; co < src_bin.cols; co++) {
                if ((bool)*pix && !isodd) {
                    sp.push_back(ro + 1 + offset);
                    isodd = !isodd;
                    break;
                } else if ((bool)*pix && isodd)
                    break;
                pix++;
            }
            if (co == src_bin.cols && isodd) {
                sp.insert(sp.end() - 1, ro + 1 + offset);
                isodd = !isodd;
                if (sp.size() == 2 * n)
                    break;
            }
        }
        if (isodd)
            sp.insert(sp.end() - 1, 0 + offset);
    } else if (direc == LEFT) {
        for (int co = 0; co < src_bin.cols; co++) {
            uchar* pix = src_bin.data + co;
            int ro = 0;
            for (; ro < src_bin.rows; ro++) {
                if ((bool)*pix && !isodd) {
                    sp.push_back(co + offset);
                    isodd = !isodd;
                    break;
                } else if ((bool)*pix && isodd)
                    break;
                pix = pix + src_bin.step;
            }
            if (ro == src_bin.rows && isodd) {
                sp.push_back(co + offset);
                isodd = !isodd;
                if (sp.size() == 2 * n)
                    break;
            }
        }
        if (isodd)
            sp.push_back(src_bin.cols + offset);
    } else if (direc == RIGHT) {
        for (int co = src_bin.cols - 1; co >= 0; co--) {
            uchar* pix = src_bin.data + co;
            int ro = 0;
            for (; ro < src_bin.rows; ro++) {
                if ((bool)*pix && !isodd) {
                    sp.push_back(co + 1 + offset);
                    isodd = !isodd;
                    break;
                } else if ((bool)*pix && isodd)
                    break;
                pix = pix + src_bin.step;
            }
            if (ro == src_bin.rows && isodd) {
                sp.insert(sp.end() - 1, co + 1 + offset);
                isodd = !isodd;
                if (sp.size() == 2 * n)
                    break;
            }
        }
        if (isodd)
            sp.insert(sp.end() - 1, 0 + offset);
    }
    return sp;
}

void squarize(Mat& src_bin)
{
    if (src_bin.empty())
        throw invalid_argument("squarize(): Image is empty");
    if (src_bin.channels() != 1)
        throw invalid_argument("squarize(): Require image in graystyle");

    int h = src_bin.rows, w = src_bin.cols;
    if (h > w) {
        int d = h - w;
        hconcat(Mat(h, d / 2, CV_8UC1, Scalar(0)), src_bin, src_bin);
        hconcat(src_bin, Mat(h, d - d / 2, CV_8UC1, Scalar(0)), src_bin);
    }
    if (w > h) {
        int d = w - h;
        vconcat(Mat(d / 2, w, CV_8UC1, Scalar(0)), src_bin, src_bin);
        vconcat(src_bin, Mat(d - d / 2, w, CV_8UC1, Scalar(0)), src_bin);
    }
}

string shash16(Mat src_bin)
{
    if (src_bin.empty())
        throw invalid_argument("shash16(): Image is empty");
    if (src_bin.channels() != 1)
        throw invalid_argument("shash16(): Require image in graystyle");

    resize(src_bin, src_bin, Size(16, 16), 0, 0);
    stringstream hash_value;
    uchar* pix = src_bin.data;
    int tmp_dec = 0;
    for (int ro = 0; ro < 256; ro++) {
        tmp_dec = tmp_dec << 1;
        if ((bool)*pix)
            tmp_dec++;
        if (ro % 4 == 3) {
            hash_value << hex << tmp_dec;
            tmp_dec = 0;
        }
        pix++;
    }
    return hash_value.str();
}

int hamming(string hash1_str, string hash2_str)
{
    stringstream hash1, hash2;
    hash1 << setw(64) << setfill('0') << hash1_str;
    hash2 << setw(64) << setfill('0') << hash2_str;
    hash1_str = hash1.str();
    hash2_str = hash2.str();
    int diff = 0;
    for (int i = 0; i < 64; i = i + 7) {
        int x = stoi(hash1_str.substr(i, 7), NULL, 16)
            ^ stoi(hash2_str.substr(i, 7), NULL, 16);
        while (x) {
            diff++;
            x = x & (x - 1);
        }
    }
    return diff;
}

class Result {
public:
    static auto analyse(Mat img, bool ex);

private:
    bool ex;
    bool valid = false;
    Mat img;
    Mat img_gray;
    Mat img_bin127;
    Mat img_bin230;
    Mat img_autobin;
    Rect baseline_v;
    Rect baseline_h;
    int item_diameter;
    string stage_code;
    dict droptypes;
    vector<dict> drops;
    dict drops_display;
    dict rectangles;

    Rect get_baseline_v();
    Rect get_baseline_h();
    Rect get_baseline_h_ex();
    bool is_result();
    bool is_3stars();
    void get_stage();
    void get_droptypes();
    void get_drops();
};

auto Result::analyse(Mat img, bool ex = false)
{
    if (img.empty())
        throw invalid_argument("analyse(): Image is empty");
    if (img.channels() != 3)
        throw invalid_argument("analyse(): Require image in BGR");

    Result result;
    result.img = img;
    // img_gray
    cvtColor(img, result.img_gray, COLOR_BGR2GRAY);
    // img_bin127
    threshold(result.img_gray, result.img_bin127, 127, 255, THRESH_BINARY);
    // baseline_v
    result.baseline_v = result.get_baseline_v();

    if (result.is_result()) {
        // img_bin230
        threshold(result.img_gray, result.img_bin230, 230, 255, THRESH_BINARY);
        // get_stage
        result.get_stage();
        {
            // ********** EXPERIMENTAL **********
            // Fix some wrong results like "O-7"
            // May add regex
            if (result.stage_code.substr(0, 2) == "O-") {
                result.stage_code.replace(0, 1, "0");
            }
            // **********************************
        }
        if (result.is_3stars()) {
            // img_autobin
            adaptiveThreshold(result.img_gray, result.img_autobin, 255, ADAPTIVE_THRESH_MEAN_C,
                THRESH_BINARY, result.baseline_v.height / 2 * 2 + 1, -20);
            // if (ex)
            result.baseline_h = result.get_baseline_h_ex();
            // else
            //     baseline_h = Result::get_baseline_h(img_bin127, baseline_v);
            if (!result.baseline_h.empty()) {
                result.valid = true;
                result.ex = ex;

                result.item_diameter = round(result.baseline_v.height * 0.5238);

                result.get_droptypes();
                result.get_drops();
                dict data = {
                    { "drops", result.drops },
                    { "stageId", stage_index[result.stage_code]["stageId"] }
                };
                auto display = tuple(result.stage_code, result.drops_display);
                return tuple(data, display, result.rectangles);
            } else {
                result.valid = false;
                return tuple(dict(), tuple(string(), dict()), dict());
            }
        } else {
            result.valid = false;
            return tuple(dict(), tuple(string(), dict()), dict());
        }
    } else {
        result.valid = false;
        return tuple(dict(), tuple(string(), dict()), dict());
    }
}

Rect Result::get_baseline_v()
{
    Mat img_bin = img_bin127;
    int column = 0;
    int top = 0, bottom = 0;

    int _start = img_bin.cols / 4;
    if (ex)
        _start = 0;
    for (int co = _start; co < img_bin.cols / 2; co++) {
        uchar* pix = img_bin.data + (img_bin.rows - 1) * img_bin.step + co;
        int begin = 0, end = 0;
        bool prober[2] = { false, false };
        for (int ro = img_bin.rows; ro > img_bin.rows / 2; ro--) {
            prober[0] = prober[1];
            prober[1] = (bool)*pix;
            if (prober[0] == false && prober[1] == true)
                end = ro;
            if (prober[0] == true && prober[1] == false)
                begin = ro;
            if (begin && end)
                break;
            pix = pix - img_bin.step;
        }
        if ((begin && end) && (end - begin > bottom - top)) {
            column = co;
            top = begin;
            bottom = end;
        }
    }
    // rectangles["baseline_v"] = { column, top, 1, bottom - top };
    return Rect(column, top, 1, bottom - top);
}

Rect Result::get_baseline_h()
{
    Mat img_bin = img_bin127;
    int min_d = baseline_v.height / 2;
    int row = 0;
    int left = 0, right = 0;
    for (int ro = baseline_v.y + baseline_v.height - 1;
         ro > baseline_v.y + baseline_v.height / 2;
         ro--) {
        uchar* pix = img_bin.data + baseline_v.x + 5 + ro * img_bin.step;
        int begin = 0, end = 0;
        bool prober[2] = { false, false };
        for (int co = baseline_v.x + 5; co < img_bin.cols; co++) {
            prober[0] = prober[1];
            prober[1] = (bool)*pix;
            if (prober[0] == false && prober[1] == true)
                begin = co;
            if (prober[0] == true && prober[1] == false)
                end = co;
            if (begin && end)
                break;
            pix++;
        }
        if ((begin && end) && (end - begin > right - left)) {
            row = ro;
            left = begin;
            right = end;
        }
        if (prober[0] == false && prober[1] == false && right - left > min_d)
            break;
    }
    // rectangles["baseline_h"] = { left, row, right - left, 1 };
    return Rect(left, row, right - left, 1);
}

Rect Result::get_baseline_h_ex()
{
    Mat img_bin = img_bin127;
    int min_d = baseline_v.height / 2;
    int row = 0;
    int count, maxcount = 0;
    for (int ro = baseline_v.y + baseline_v.height - 1;
         ro > baseline_v.y + 3 * baseline_v.height / 4;
         ro--) {
        uchar* pix = img_bin.data + baseline_v.x + 5 + ro * img_bin.step;
        count = 0;
        for (int co = baseline_v.x + 5; co < img_bin.cols; co++) {
            if ((bool)*pix)
                count++;
            pix++;
        }
        if (count > maxcount) {
            row = ro;
            maxcount = count;
        }
    }
    // rectangles["baseline_h"] = { baseline_v.x + 5, row, min_d, 1 };
    return Rect(baseline_v.x + 5, row, min_d, 1);
}

bool Result::is_result()
{
    Mat img_bin = img_bin127;
    Mat resultimg = img_bin(Rect(
        0, baseline_v.y + baseline_v.height / 2,
        baseline_v.x - 5, baseline_v.height / 2));
    Rect rect_result = boundingRect(resultimg);
    resultimg = resultimg(rect_result);
    string hash = shash16(resultimg);
    int dist = hamming(hash, hash_index["result"]["zh"]);
    if (dist <= 50)
        return true;
    else
        return false;
}

bool Result::is_3stars()
{
    Mat img_bin = img_bin127;
    Rect rect_stars;
    rect_stars.x
        = (int)rectangles["stage"][X] + (int)rectangles["stage"][WIDTH] + 1;
    rect_stars.y = baseline_v.y;
    rect_stars.width = baseline_v.x - 5 - rect_stars.x;
    rect_stars.height = baseline_v.height / 4;
    // rectangles["stars"] = {
    //     rect_stars.x, rect_stars.y, rect_stars.width, rect_stars.height * 2
    // };
    Mat starsimg = img_bin(rect_stars);
    vector sp = separate(starsimg, LEFT);
    if (sp.size() == 6)
        return true;
    else
        return false;
}

void Result::get_stage()
{
    auto get_char = [](Mat charimg) {
        if (charimg.empty())
            throw invalid_argument("get_stage(): Image is empty");

        string charhash = shash16(charimg);
        string chr;
        int diff = 64;
        for (auto& [kchar, vhash] : hash_index["stage"].items()) {
            int d = hamming(charhash, vhash);
            if (d < diff) {
                if (d == 0)
                    return kchar;
                chr = kchar;
                diff = d;
            }
        }
        return chr;
    };

    string stage = "";
    Mat stageimg = img_bin230(Rect(
        0, baseline_v.y,
        baseline_v.x - 5, baseline_v.height / 4));
    Rect rect_stage = boundingRect(stageimg);
    rectangles["stage"] = {
        rect_stage.x, rect_stage.y, rect_stage.width, rect_stage.height
    };
    stageimg = stageimg(rect_stage);
    vector sp = separate(stageimg, LEFT);
    if (!sp.empty()) {
        for (int i = 0; i < sp.size(); i = i + 2) {
            Mat charimg = stageimg(Rect(
                sp[i], 0, sp[i + 1] - sp[i], stageimg.rows));
            charimg = charimg(boundingRect(charimg));
            squarize(charimg);
            stage = stage + get_char(charimg);
        }
    }
    stage_code = stage;
}

void Result::get_droptypes()
{
    auto get_hsv = [](Scalar bgra) {
        double b = (bgra[0]) / 255;
        double g = (bgra[1]) / 255;
        double r = (bgra[2]) / 255;
        double cmax = std::max({ b, g, r });
        double cmin = std::min({ b, g, r });
        double delta = cmax - cmin;
        double h, s, v;
        if (delta < 1e-15)
            h = 0;
        else if (cmax == r)
            h = 60 * ((g - b) / delta);
        else if (cmax == g)
            h = 60 * ((b - r) / delta) + 120;
        else if (cmax == b)
            h = 60 * ((r - g) / delta) + 240;
        if (h < 0)
            h = h + 360;
        if (cmax == 0)
            s = 0;
        else
            s = delta / cmax;
        v = cmax;
        return tuple(h, s, v);
    };

    auto get_type = [this, get_hsv](Mat droplineimg) {
        string droptype;
        Scalar_<int> bgra = mean(droplineimg);
        auto [h, s, v] = get_hsv(bgra);
        if (s < 0.05)
            droptype = "NORMAL_DROP";
        else {
            int dist = 360;
            for (auto& [kh, vtype] : hsv_index.items()) {
                int d = abs(stoi(kh) - h);
                if (d < dist) {
                    dist = d;
                    droptype = vtype;
                }
            }
        }
        if (droptype == "SPECIAL_DROP" || droptype == "FURNITURE") {
            Size wholesize;
            Point upperleft;
            droplineimg.locateROI(wholesize, upperleft);
            Mat droptypeimg = img_gray(
                Rect(upperleft.x, upperleft.y + 1, droplineimg.cols / 2,
                    baseline_v.y + baseline_v.height - upperleft.y))
                                  .clone();
            cvtColor(droplineimg, droplineimg, COLOR_BGR2GRAY);
            double minval, maxval;
            Point minloc, maxloc;
            minMaxLoc(droplineimg, &minval, &maxval, &minloc, &maxloc);
            threshold(droptypeimg, droptypeimg, maxval / 2, 255, THRESH_BINARY);
            bool overline = true;
            while (overline) {
                uchar* pix = droptypeimg.data;
                int count = 0;
                for (int co = 0; co < droptypeimg.cols; co++) {
                    if ((bool)*pix) {
                        count++;
                    }
                    pix++;
                }
                if (count > droptypeimg.cols / 2) {
                    droptypeimg = droptypeimg(Rect(
                        0, 1, droptypeimg.cols, droptypeimg.rows - 1));
                } else
                    overline = false;
            }
            droptypeimg = droptypeimg(boundingRect(droptypeimg));

            string hash_value = shash16(droptypeimg);
            int dist_special = hamming(hash_value,
                hash_index["droptype"]["zh"]["SPECIAL_DROP"]);
            int dist_furni = hamming(hash_value,
                hash_index["droptype"]["zh"]["FURNITURE"]);
            if (dist_special < dist_furni)
                droptype = "SPECIAL_DROP";
            else if (dist_furni < dist_special)
                droptype = "FURNITURE";
            else
                droptype = "UNDEFINED";
        }
        return droptype;
    };

    Mat img_bin;
    // if (ex)
    img_bin = img_autobin;
    // else
    //     img_bin = img_bin127;
    vector sp = separate(
        img_bin(Rect(baseline_v.x + 5, baseline_h.y,
            img.cols - (baseline_v.x + 5), 1)),
        LEFT, 0, true);
    { //fill the gap if it is smaller than threshold
        int i = 1;
        while (i + 1 < sp.size()) {
            if (sp[i + 1] - sp[i] < 5)
                sp.erase(sp.begin() + i, sp.begin() + i + 2);
            else
                i = i + 2;
        }
    }
    { //delete the line if it is smaller than threshold
        int i = 0;
        while (i < sp.size()) {
            if (sp[i + 1] - sp[i] < item_diameter)
                sp.erase(sp.begin() + i, sp.begin() + i + 2);
            else
                i = i + 2;
        }
    }
    { // predict the line if there is a gap > item diameter
        int i = 1;
        while (i + 1 < sp.size()) {
            if (sp[i + 1] - sp[i] > item_diameter) {
                int midpoint = (sp[i] + sp[i + 1]) / 2;
                int count = (sp[i + 2] - sp[i + 1]) / item_diameter;
                int length = (sp[i + 2] - sp[i + 1]) / count;
                sp.insert(sp.begin() + i + 1, midpoint - length / 2);
                sp.insert(sp.begin() + i + 2, midpoint + length / 2);
            }
            i = i + 2;
        }
    }

    int nodes = sp.size();
    if (nodes) {
        for (int i = 0; i < nodes; i = i + 2) {
            Mat droplineimg = img(Rect(
                sp[i], baseline_h.y, sp[i + 1] - sp[i], 1));
            string droptype = get_type(droplineimg);
            if (droptype == "FIRST") {
                valid = false;
                return;
            }
            droptypes.push_back({ droptype, { sp[i], sp[i + 1] } });
        }
        int lasttype = -1;
        for (auto& [idx, droptype] : droptypes.items()) {
            int currenttype = droptype_order[(string)droptype[0]];
            if (currenttype <= lasttype) {
                // ********** EXPERIMENTAL **********
                {
                    if (lasttype == 5) {
                        droptypes[stoi(idx) - 1][0] = "LMB";
                        lasttype = currenttype;
                        // throw a warning
                        continue;
                    }
                }
                // **********************************

                valid = false;
                return;
            } else
                lasttype = currenttype;
        }
    } else {
        valid = false;
        return;
    }
}

void Result::get_drops()
{
    auto detect_item = [this](Mat& dropimg) {
        auto get_mask = [](Mat src) {
            Mat src_gray, mask;
            cvtColor(src, src_gray, COLOR_BGR2GRAY);
            threshold(src_gray, mask, 0, 255, THRESH_BINARY);
            cvtColor(mask, mask, COLOR_GRAY2BGR);
            return mask;
        };

        string itemId;
        double similarity = 0;
        Point upperleft;
        for (auto& item : stage_index[stage_code]["drops"].items()) {
            string itemid = item.value();

            Mat templ = templs[item_index[itemid]["img"]];
            double fx = (double)item_diameter / 163;
            resize(templ, templ, Size(), fx, fx);
            Mat mask = get_mask(templ);
            Mat resimg;
            matchTemplate(dropimg, templ, resimg, TM_CCORR_NORMED, mask);

            double minval, maxval;
            Point minloc, maxloc;
            minMaxLoc(resimg, &minval, &maxval, &minloc, &maxloc);
            if (maxval > similarity) {
                itemId = itemid;
                similarity = maxval;
                upperleft = maxloc;
            }
        }
        int w = round(183 * ((double)item_diameter / 163));
        dropimg = dropimg(Rect(upperleft.x, upperleft.y, w, w));
        return tuple(itemId, similarity);
    };
    auto detect_quantity = [](Mat dropimg) {
        auto get_char = [](Mat charimg) {
            if (charimg.empty())
                throw invalid_argument("detect_quantity(): Image is empty");

            string charhash = shash16(charimg);
            string chr;
            int diff = 256;
            for (auto& [kchar, vhash] : hash_index["dropimg"]["zh"].items()) {
                int d = hamming(charhash, vhash);
                if (d < diff) {
                    if (d == 0)
                        return kchar;
                    chr = kchar;
                    diff = d;
                }
            }
            return chr;
        };

        int height = dropimg.rows;
        int width = dropimg.cols;
        Mat qimg = dropimg(Rect(
            0, round(height * 0.7),
            round(width * 0.82), round(height * 0.15)));

        int digits;
        vector sp = separate(qimg, RIGHT);
        int edge = qimg.cols - sp[1] - 1;
        for (int i = 3; i < sp.size(); i = i + 2) {
            if (abs(sp[i] - sp[i - 3]) >= edge) {
                digits = i / 2;
                break;
            }
        }

        string quantity = "";
        for (int i = (digits - 1) * 2; i >= 0; i = i - 2) {
            Mat charimg = qimg(Rect(
                sp[i], 0, sp[i + 1] - sp[i], qimg.rows));
            charimg = charimg(boundingRect(charimg));
            squarize(charimg);
            quantity = quantity + get_char(charimg);
        }
        return quantity;
    };

    if (!droptypes.empty()) {
        for (auto& droptype : droptypes) {
            string ktype = droptype[0];
            auto vrange = droptype[1];
            if (ktype == "LMB")
                continue;
            else if (ktype == "FURNITURE") {
                drops.push_back({ { "dropType", ktype },
                    { "itemId", "furni" },
                    { "quantity", 1 } });
                drops_display[ktype] = { true };
                continue;
            } else
                drops_display[ktype] = {};

            int typerange[] = { (int)vrange[BEGIN], (int)vrange[END] };
            int typerange_len = typerange[END] - typerange[BEGIN];
            int count = typerange_len / item_diameter;
            for (int i = 0; i < count; i++) {
                int range[] = {
                    typerange[BEGIN] + i * typerange_len / count,
                    typerange[BEGIN] + (i + 1) * typerange_len / count
                };
                int offset = (range[END] - range[BEGIN] - item_diameter) / 3;
                Mat dropimg = img(
                    Range(baseline_v.y + baseline_v.height / 4, baseline_h.y),
                    Range(range[BEGIN], range[END]));
                auto [itemId, similarity] = detect_item(dropimg);

                if (similarity > 0.9) {
                    Mat dropimg_bin;
                    cvtColor(dropimg, dropimg_bin, COLOR_BGR2GRAY);
                    threshold(dropimg_bin, dropimg_bin, 127, 255, THRESH_BINARY);
                    string quantity = detect_quantity(dropimg_bin);
                    drops.push_back({ { "dropType", ktype },
                        { "itemId", itemId },
                        { "quantity", quantity } });
                    string name = item_index[itemId]["name_i18n"]["zh"];
                    drops_display[ktype][name] = quantity;

                    Size wholesize;
                    Point upperleft;
                    dropimg.locateROI(wholesize, upperleft);
                    rectangles["drops"][itemId] = {
                        upperleft.x, upperleft.y, dropimg.cols, dropimg.rows
                    };
                }
            }
        }
    }
}

Mat decode(uint8_t* buffer, size_t size)
{
    vector buf(buffer, buffer + size);
    return imdecode(buf, 1);
}

extern "C" {
void preload_json(char* stagex, char* itemx, char* hashx)
{
    stage_index = dict::parse(stagex);
    item_index = dict::parse(itemx);
    hash_index = dict::parse(hashx);
}
}

extern "C" {
void preload_templ(char* itemId, uint8_t* buffer, size_t size)
{
    Mat img = decode(buffer, size);
    item_index[itemId]["img"] = templs.size();
    templs.push_back(img);
}
}

extern "C" {
const char* recognize(uint8_t* buffer, size_t size, int ex)
{
    Mat img = decode(buffer, size);

    auto s = getTickCount();
    if (!ex) {
        if (img.rows > 600) {
            double fx = 600.0 / img.rows;
            resize(img, img, Size(), fx, fx, INTER_AREA);
        }
    }

    auto [data, display, rectangles] = Result::analyse(img, ex);
    auto e = getTickCount();
    cout << (e - s) / getTickFrequency() * 1000 << endl;
    free(buffer);

    dict res_dict = { data, rectangles };
    string res_str = res_dict.dump();
    char* res = new char[res_str.length() + 1];
    strcpy(res, res_str.c_str());
    return res;
}
}