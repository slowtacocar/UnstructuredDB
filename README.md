# UnstructuredDB

This is a super basic example of an unstructured in-memory database written in pure C. Supports inserts and selects with indexed columns. This is just a learning experience/example, no practical purpose.

This involves an obnoxious number of hash tables to handle looking up indexes by column name, looking up indexed values, and retrieving specific keys from documents.

"Queries" on this "database" copy the data into a simpler(?) structure with most of the data in a contiguous block of memory (the Table from my StructuredDB project).
