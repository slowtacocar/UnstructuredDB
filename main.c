#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

enum type {
    INT,
    STRING,
    BOOL,
};

#define cast(type, value) ( \
    (type) == INT ? *(int *) (value) :   \
    (type) == STRING ? *(char **) (value) :  \
    (type) == BOOL ? *(bool *) (value) : 0)
#define formatter(type) (type) == INT ? "%d" : (type) == STRING ? "%s" : (type) == BOOL ? "%d" : 0
#define hashType(type, value) ((type) == INT ? *(unsigned int *) (value) : (type) == STRING ? hashString(*(char **) (value)) : (type) == BOOL ? *(unsigned int *) (value) : 0)
#define hashCompareType(type, value) ((type) == INT ? *(unsigned int *) (value) : (type) == STRING ? hashString((char *) (value)) : (type) == BOOL ? *(unsigned int *) (value) : 0)
#define compareType(type, value1, value2) ((type) == INT ? *(int *) (value1) == *(int *) (value2) : (type) == STRING ? strcmp((char *) (value1), *(char **) (value2)) == 0 : (type) == BOOL ? *(bool *) (value1) == *(bool *) (value2) : 0)
#define sizeOfType(type) (type) == INT ? sizeof(int) : (type) == STRING ? sizeof(char *) : (type) == BOOL ? sizeof(bool) : 0

struct InsertDef {
    char *columnName;
    void *value;
    enum type type;
};

struct HashTable {
    void **entries;
    size_t numRows;
};

struct Column {
    size_t start;
    size_t size;
    char *name;
    enum type type;
    struct HashTable index;
};

struct Table {
    void *entries;
    size_t numRows;
    size_t allocatedRows;
    size_t rowSize;
    struct HashTable colLocs;
    size_t numColumns;
    struct Column **columnDefs;
};

struct Value {
    void *value;
    enum type type;
};

struct Collection {
    struct HashTable *entries;
    size_t numRows;
    size_t allocatedRows;
    struct HashTable indexes;
};

struct SelectDef {
    char *columnName;
    enum type type;
};

unsigned int powi(unsigned int a, unsigned int b) {
    unsigned int ret = 1;
    for (unsigned int i = 0; i < b; i++) {
        ret *= a;
    }
    return ret;
}

unsigned int hashString(const char *string) {
    unsigned int ret = 0;
    for (size_t i = 0; string[i] != '\0'; i++) {
        ret += string[i] * powi(31, i);
    }
    return ret;
}

struct HashTable newHashTable(size_t size) {
    struct HashTable ret = {
            .entries = malloc(sizeof(void *) * size * 2),
            .numRows = size,
    };
    return ret;
}

void *getByKey(const char *key, struct HashTable *table) {
    unsigned int hash = hashString(key) % table->numRows;
    while (table->entries[hash * 2] != NULL) {
        if (strcmp(key, table->entries[hash * 2]) == 0) {
            return table->entries[hash * 2 + 1];
        }
        hash = (hash + 1) % table->numRows;
    }
    return NULL;
}

unsigned int countByKey(enum type type, void *key, struct HashTable *table) {
    unsigned int hash = hashCompareType(type, key) % table->numRows;
    unsigned int count = 0;
    while (table->entries[hash * 2] != NULL) {
        if (compareType(type, key, table->entries[hash * 2])) {
            count++;
        }
        hash = (hash + 1) % table->numRows;
    }
    return count;
}

void putKey(unsigned int h, void *key, void *value, struct HashTable *table) {
    unsigned int hash = h % table->numRows;
    while (table->entries[hash * 2] != NULL) {
        hash = (hash + 1) % table->numRows;
    }
    table->entries[hash * 2] = key;
    table->entries[hash * 2 + 1] = value;
}

struct Collection newCollection(char *indexedColumns[], int numIndexedColumns) {
    struct HashTable indexes = newHashTable(64);
    for (int i = 0; i < numIndexedColumns; i++) {
        struct HashTable *index = malloc(sizeof(struct HashTable));
        *index = newHashTable(64);
        putKey(hashString(indexedColumns[i]), indexedColumns[i], index, &indexes);
    }
    struct HashTable *index = malloc(sizeof(struct HashTable));
    *index = newHashTable(64);
    putKey(hashString("id"), "id", index, &indexes);
    struct Collection ret = {
            .entries = malloc(sizeof(struct HashTable *) * 128),
            .numRows = 0,
            .allocatedRows = 128,
            .indexes = indexes,
    };
    return ret;
}

void insert(struct Collection *collection, struct InsertDef inserts[], int numInserts) {
    if (collection->numRows >= collection->allocatedRows) {
        collection->entries = realloc(collection->entries, sizeof(struct HashTable *) * (collection->allocatedRows * 2));
        collection->allocatedRows *= 2;
    }
    struct HashTable *document = malloc(sizeof(struct HashTable));
    *document = newHashTable(64);
    int *id = malloc(sizeof(size_t));
    *id = (int) collection->numRows;
    struct Value *idValue = malloc(sizeof(struct Value));
    idValue->value = id;
    idValue->type = INT;
    putKey(hashString("id"), "id", idValue, document);
    putKey(*id, id, document, getByKey("id", &collection->indexes));
    for (int i = 0; i < numInserts; i++) {
        struct Value *value = malloc(sizeof(struct Value));
        value->value = inserts[i].value;
        value->type = inserts[i].type;
        putKey(hashString(inserts[i].columnName), inserts[i].columnName, value, document);
        struct HashTable *index = getByKey(inserts[i].columnName, &collection->indexes);
        if (index) {
            putKey(hashType(inserts[i].type, inserts[i].value), inserts[i].value, document, getByKey(inserts[i].columnName, &collection->indexes));
        }
    }
    collection->entries[collection->numRows++] = *document;
}

struct Table select(struct Collection *collection, struct SelectDef selects[], int numSelects) {
    struct Column **selectDefs = malloc(sizeof(struct Column *) * numSelects);
    struct HashTable colLocs = newHashTable(64);
    size_t selectSize = 0;
    for (int i = 0; i < numSelects; i++) {
        struct Column *selectCol = malloc(sizeof(struct Column));
        selectCol->start = selectSize;
        selectCol->size = sizeOfType(selects[i].type);
        selectCol->name = selects[i].columnName;
        selectCol->type = selects[i].type;
        selectSize += selectCol->size;
        selectDefs[i] = selectCol;
        putKey(hashString(selects[i].columnName), selects[i].columnName, (void *) selectCol, &colLocs);
    }
    void *ret = malloc(selectSize * collection->numRows);
    for (int i = 0; i < collection->numRows; i++) {
        void *selectRow = ret + selectSize * i;
        for (int j = 0; j < numSelects; j++) {
            struct Value *value = getByKey(selects[j].columnName, &collection->entries[i]);
            memcpy(selectRow + selectDefs[j]->start, value->value, sizeOfType(value->type));
        }
    }
    struct Table retTable = {
            .entries = ret,
            .numRows = collection->numRows,
            .allocatedRows = collection->numRows,
            .rowSize = selectSize,
            .colLocs = colLocs,
            .columnDefs = selectDefs,
            .numColumns = numSelects,
    };
    return retTable;
}

struct Table selectWhere(struct Collection *collection, struct SelectDef selects[], int numSelects, struct SelectDef where, void *value) {
    struct Column **selectDefs = malloc(sizeof(struct Column *) * numSelects);
    struct HashTable colLocs = newHashTable(64);
    size_t selectSize = 0;
    for (int i = 0; i < numSelects; i++) {
        struct Column *selectCol = malloc(sizeof(struct Column));
        selectCol->start = selectSize;
        selectCol->size = sizeOfType(selects[i].type);
        selectCol->name = selects[i].columnName;
        selectCol->type = selects[i].type;
        selectSize += selectCol->size;
        selectDefs[i] = selectCol;
        putKey(hashString(selects[i].columnName), selects[i].columnName, (void *) selectCol, &colLocs);
    }
    struct HashTable *index = getByKey(where.columnName, &collection->indexes);
    unsigned int count = countByKey(where.type, value, index);
    void *ret = malloc(selectSize * count);
    unsigned int hash = hashCompareType(where.type, value) % index->numRows;
    unsigned int k = 0;
    while (index->entries[hash * 2] != NULL) {
        if (compareType(where.type, value, index->entries[hash * 2])) {
            void *selectRow = ret + selectSize * k;
            for (int j = 0; j < numSelects; j++) {
                struct Value *val = getByKey(selects[j].columnName, index->entries[hash * 2 + 1]);
                memcpy(selectRow + selectDefs[j]->start, val->value, sizeOfType(val->type));
            }
            k++;
        }
        hash = (hash + 1) % index->numRows;
    }
    struct Table retTable = {
            .entries = ret,
            .numRows = count,
            .allocatedRows = count,
            .rowSize = selectSize,
            .colLocs = colLocs,
            .columnDefs = selectDefs,
            .numColumns = numSelects,
    };
    return retTable;
}

struct Table selectByID(struct Collection *collection, struct SelectDef selects[], int numSelects, int id) {
    struct SelectDef where = {.columnName = "id", .type = INT};
    return selectWhere(collection, selects, numSelects, where, &id);
}

void printTable(struct Table *table) {
    for (int i = 0; i < table->numColumns; i++) {
        printf("%s\t", table->columnDefs[i]->name);
    }
    printf("\n");
    for (int i = 0; i < table->numRows; i++) {
        void *row = table->entries + table->rowSize * i;
        for (int j = 0; j < table->numColumns; j++) {
            printf(formatter(table->columnDefs[j]->type), cast(table->columnDefs[j]->type, row + table->columnDefs[j]->start));
            printf("\t");
        }
        printf("\n");
    }
}

int main() {
    struct Collection collection = newCollection((char *[]) {"name"}, 1);
    char *name = "John Doe";
    struct InsertDef inserts[] = {
            {.columnName = "name", .value = &name, .type = STRING},
            {.columnName = "age", .value = &(int) {42}, .type = INT},
    };
    insert(&collection, inserts, 2);
    char *name2 = "Jane Doe";
    struct InsertDef inserts2[] = {
            {.columnName = "name", .value = &name2, .type = STRING},
            {.columnName = "age", .value = &(int) {43}, .type = INT},
    };
    insert(&collection, inserts2, 2);
    char *name3 = "John Smith";
    struct InsertDef inserts3[] = {
            {.columnName = "name", .value = &name3, .type = STRING},
            {.columnName = "age", .value = &(int) {44}, .type = INT},
    };
    insert(&collection, inserts3, 2);
    char *name4 = "Jane Smith";
    struct InsertDef inserts4[] = {
            {.columnName = "name", .value = &name4, .type = STRING},
            {.columnName = "age", .value = &(int) {45}, .type = INT},
    };
    insert(&collection, inserts4, 2);
    char *name5 = "John Doe";
    struct InsertDef inserts5[] = {
            {.columnName = "name", .value = &name5, .type = STRING},
            {.columnName = "age", .value = &(int) {46}, .type = INT},
    };
    insert(&collection, inserts5, 2);

    printf("All entries:\n");
    struct SelectDef selects[] = {
            {.columnName = "id", .type = INT},
            {.columnName = "name", .type = STRING},
            {.columnName = "age", .type = INT},
    };
    struct Table sel1 = select(&collection, selects, 3);
    printTable(&sel1);

    printf("\nID = 3:\n");
    struct Table sel2 = selectByID(&collection, selects, 3, 3);
    printTable(&sel2);

    printf("\nname = John Doe\n");
    struct SelectDef where = {.columnName = "name", .type = STRING};
    struct Table sel3 = selectWhere(&collection, selects, 3, where, "John Doe");
    printTable(&sel3);
}
