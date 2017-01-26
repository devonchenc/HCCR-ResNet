#include <iostream>
#include <fstream>
#include <memory>
#include <math.h>
#include <dirent.h>

using namespace std;

#include <google/protobuf/text_format.h>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "caffe/proto/caffe.pb.h"
#include "caffe/util/db.hpp"
#include "caffe/util/format.hpp"

using namespace google;
using namespace caffe;
using boost::scoped_ptr;
using boost::shared_ptr;

#pragma pack(1)
struct Header
{
    unsigned int size;
    union
    {
        char tagCode[2];
        unsigned short label;
    };
    unsigned short width;
    unsigned short height;
};

#define NEW_WIDTH		64
#define NEW_HEIGHT		64
#define MARGIN			4

void process(unsigned char* bitmap, unsigned short width, unsigned short height,
            unsigned char* new_bitmap, unsigned short new_width, unsigned short new_height, unsigned short margin)
{
    unsigned short roi_width = new_width - 2 * margin;
    unsigned short roi_height = new_height - 2 * margin;
    float* float_data = new float[roi_width * roi_height];
    memset(float_data, 0, sizeof(float) * roi_width * roi_height);

    // resizing
    for (unsigned short j = 0; j < roi_height; j++)
    {
        for (unsigned short i = 0; i < roi_width; i++)
        {
            // interpolation
            float fx = float(i) / float(roi_width - 1) * float(width - 2) + 0.5f;		// [0.5, width-1.5f]
            float fy = float(j) / float(roi_height - 1) * float(height - 2) + 0.5f;

            unsigned short x = (unsigned short)floor(fx);
            unsigned short y = (unsigned short)floor(fy);
            float x_decimal = fx - x;
            float y_decimal = fy - y;

            float temp1 = bitmap[y * width + x] * (1 - x_decimal) + bitmap[y * width + x + 1] * x_decimal;
            float temp2 = bitmap[(y + 1) * width + x] * (1 - x_decimal) + bitmap[(y + 1) * width + x + 1] * x_decimal;

            float_data[j * roi_width + i] = temp1 * (1 - y_decimal) + temp2 * y_decimal;
        }
    }

    // find max and min value
    float min = float_data[0];
    float max = float_data[0];
    for (unsigned short i = 1; i < roi_width * roi_height; i++)
    {
        if (min > float_data[i])
        {
            min = float_data[i];
        }
        if (max < float_data[i])
        {
            max = float_data[i];
        }
    }

    // contrast maximization
    for (unsigned short j = 0; j < roi_height; j++)
    {
        for (unsigned short i = 0; i < roi_width; i++)
        {
            new_bitmap[(j + margin) * new_width + i + margin] = (unsigned char)((float_data[j * roi_width + i] - min) / (max - min) * 255.0f);
        }
    }

    delete[] float_data;
}

int convert_HWDB(const char* image_filename, shared_ptr<db::Transaction> txn, int start_index,
                 map<unsigned short, unsigned short>& character_map, int subclass)
{
    ifstream in(image_filename, ios::in | ios::binary);
    if (!in.is_open())
    {
        cout << "file open error!\n";
        return 0;
    }

    unsigned char* new_bitmap = new unsigned char[NEW_WIDTH * NEW_HEIGHT];

    // Storing to db
    int count = 0;

    while (in.peek() != EOF)
    {
        Header head;
        in.read((char*)&head, sizeof(Header));
        CHECK_EQ(sizeof(Header) + head.width * head.height, head.size) << "file header size error\n";

        unsigned char* bitmap = new unsigned char[head.width * head.height];
        in.read((char*)bitmap, sizeof(unsigned char) * head.width * head.height);
        streamsize read_count = in.gcount();
        CHECK_EQ(read_count, head.width * head.height) << "file size error\n";

        unsigned short index = character_map[head.label];
        if (index >= subclass)
        {
            delete[] bitmap;
            continue;
        }

        // process image
        memset(new_bitmap, 255, sizeof(unsigned char) * NEW_WIDTH * NEW_HEIGHT);
        process(bitmap, head.width, head.height, new_bitmap, NEW_WIDTH, NEW_HEIGHT, MARGIN);
        delete[] bitmap;

        // fill Datum
        Datum datum;
        datum.set_channels(1);
        datum.set_width(NEW_WIDTH);
        datum.set_height(NEW_HEIGHT);
        datum.set_data(new_bitmap, NEW_WIDTH * NEW_HEIGHT);
        datum.set_label(index);

        string key_str = caffe::format_int(start_index + count, 8);
        string value;
        datum.SerializeToString(&value);

        txn->Put(key_str, value);

        if (++count % 1000 == 0)
        {
            txn->Commit();
        }
    }

    // commit the last less than 1000 datums
    if  (count % 1000 != 0)
    {
        txn->Commit();
    }

    in.close();
    delete[] new_bitmap;

    cout << image_filename << " have been converted\n";

    return count;
}

void traverse(const char* path_name, const char* db_path, const string& db_backend,
              map<unsigned short, unsigned short>& character_map, int subclass)
{
    DIR* dir = opendir(path_name);
    if  (!dir)
        return;

    scoped_ptr<db::DB> db(db::GetDB(db_backend));
    db->Open(db_path, db::NEW);
    shared_ptr<db::Transaction> txn(db->NewTransaction());

    int count = 0;
    dirent* d_ent;
    while ((d_ent = readdir(dir)) != NULL)
    {
        if (d_ent->d_type == DT_DIR)
            continue;

        if(d_ent->d_name[0] == 'H')
            continue;

        char file_name[256];
        strcpy(file_name, path_name);
        strcat(file_name, d_ent->d_name);
        count += convert_HWDB(file_name, txn, count, character_map, subclass);
    }
    cout << "Processed " << count << " images.\n";

    db->Close();
}

bool read_character_code(const char* path_name, map<unsigned short, unsigned short>& character_map)
{
    char file_name[256];
    strcpy(file_name, path_name);
    strcat(file_name, "HWDB1.1_character.txt");
    ifstream in(file_name, ios::in);
    if (!in.is_open())
    {
        cout << "file open error!\n";
        return false;
    }

    while (in.peek() != EOF)
    {
        unsigned short index;
        unsigned short code;
        char character[2];
        in >> index;
        in >> code;
        in >> character;
        character_map[code] = index;
    }
    in.close();

    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        return -1;

    map<unsigned short, unsigned short> character_map;
    if (!read_character_code(argv[1], character_map))
        return -1;

    int subclass = 1000;    // set as 3755 for all character classes

    const string db_backend = "lmdb";	// leveldb
    traverse(argv[1], argv[2], db_backend, character_map, subclass);

    return 0;
}
