#include <cstddef>
#include <cstring>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>
#include <string>

extern "C" {
    #include "../src/storage/storage.h"
}

TEST(STORAGE_TESTS, INIT_AND_FREE) {
    storage_t *storage = init_storage();
    ASSERT_NO_FATAL_FAILURE(free_storage(storage));
}

TEST(STORAGE_TESTS, READWRITE) {
    storage_t *storage = init_storage();
    char data[] = "HELLO, LINUX WORLD!!!";
    int size = strlen(data);
    icreate(storage);
    auto *inode = static_cast<inode_t *>(get(storage->inodes, 0));
    ASSERT_GE(iwrite(inode, data, size, 0), 0);
    ASSERT_STREQ(inode->data, data); //Possible not same ptr

    auto new_data = static_cast<char *>(malloc(1024));
    ASSERT_EQ(iread(inode, new_data, size, 0), size);
    ASSERT_STREQ(new_data, data);
    free(new_data);
    free_storage(storage);
}

TEST(CATALOG_TESTS, ADDFILE) {
    storage_t *storage = init_storage();
    ASSERT_TRUE(catalog_exists(storage, "/"));
    char filepath[] = "/testfile.txt";
    catalog_add(storage, filepath, false, 1);
    ASSERT_TRUE(catalog_exists(storage, filepath));

    catalog_node_t *node = catalog_get(storage, filepath);
    auto *inode = static_cast<inode_t *>(get(storage->inodes, 0));
    ASSERT_STREQ(node->fname, "testfile.txt");
    ASSERT_EQ(node->entries_count, 0);
    ASSERT_EQ(node->inode_index, 1);
    ASSERT_EQ(node->is_dir, false);
    ASSERT_EQ(node->parent, catalog_get(storage, "/"));
    free_storage(storage);
}

TEST(CATALOG_TESTS, EMULATE) {
    storage_t *storage = init_storage();
    ASSERT_TRUE(catalog_exists(storage, "/"));
    char dirpath[] = "/testdir";
    char filepath[] = "/testdir/testfile.txt";
    catalog_add(storage, dirpath, true, 1);
    catalog_add(storage, filepath, false, 2);

    catalog_node_t *node = catalog_get(storage, filepath);
    auto *inode = static_cast<inode_t *>(get(storage->inodes, 0));
    ASSERT_STREQ(node->fname, "testfile.txt");
    ASSERT_EQ(node->entries_count, 0);
    ASSERT_EQ(node->inode_index, 2);
    ASSERT_EQ(node->is_dir, false);
    ASSERT_EQ(node->parent, catalog_get(storage, dirpath));

    ASSERT_NO_FATAL_FAILURE(catalog_erase(node, true));
    ASSERT_FALSE(catalog_exists(storage, filepath));

    catalog_add(storage, "/dir2", true, 3);
    catalog_add(storage, "/file3", false, 4);

    free_storage(storage);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}