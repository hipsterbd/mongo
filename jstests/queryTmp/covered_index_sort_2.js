// Simple covered index query test with sort on _id

var coll = db.getCollection("covered_sort_2")
coll.drop()
for (i=0;i<10;i++) {
    coll.insert({_id:i})
}
coll.insert({_id:"1"})
coll.insert({_id:{bar:1}})
coll.insert({_id:null})

// Test no query
// NEW QUERY EXPLAIN
assert.eq(coll.find({}, {_id:1}).sort({_id:-1}).hint({_id:1}).itcount(), 13); 
/* NEW QUERY EXPLAIN
assert.eq(true, plan.indexOnly, "sort.2.1 - indexOnly should be true on covered query")
*/
/* NEW QUERY EXPLAIN
assert.eq(0, plan.nscannedObjects, "sort.2.1 - nscannedObjects should be 0 for covered query")
*/

print ('all tests pass')
