set listsize 35

define hh
call mm_checkheap(1)
end

b mm_init
b mm_malloc
b mm_free
