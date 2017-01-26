add_definitions(-DUSE_LMDB)
#add_definitions(-DUSE_LEVELDB)

set(SRC_LIST db.cpp db_leveldb.cpp db_lmdb.cpp)
add_library(caffe_util ${SRC_LIST})
