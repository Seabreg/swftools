#include <assert.h>
#include "wind.h"

fillstyle_t fillstyle_default;

windstate_t windstate_nonfilled = {
    is_filled: 0,
    wind_nr: 0,

    /* TODO: move num_polygons into windstate_t.internal */
    num_polygons: 1,
};

// -------------------- even/odd ----------------------

windstate_t evenodd_start(int num_polygons)
{
    return windstate_nonfilled;
}
windstate_t evenodd_add(windstate_t left, fillstyle_t*edge, segment_dir_t dir, int master)
{
    left.is_filled ^= 1;
    return left;
}
fillstyle_t* evenodd_diff(windstate_t*left, windstate_t*right)
{
    if(left->is_filled==right->is_filled)
        return 0;
    else
        return &fillstyle_default;
}

windrule_t windrule_evenodd = {
    start: evenodd_start,
    add: evenodd_add,
    diff: evenodd_diff,
};

// -------------------- circular ----------------------

windstate_t circular_start(int num_polygons)
{
    return windstate_nonfilled;
}

windstate_t circular_add(windstate_t left, fillstyle_t*edge, segment_dir_t dir, int master)
{
    /* which one is + and which one - doesn't actually make any difference */
    if(dir == DIR_DOWN)
        left.wind_nr++;
    else
        left.wind_nr--;

    left.is_filled = left.wind_nr != 0;
    return left;
}

fillstyle_t* circular_diff(windstate_t*left, windstate_t*right)
{
    if(left->is_filled==right->is_filled)
        return 0;
    else
        return &fillstyle_default;
}

windrule_t windrule_circular = {
    start: circular_start,
    add: circular_add,
    diff: circular_diff,
};

// -------------------- intersect ----------------------

windstate_t intersect_start(int num_polygons)
{
    windstate_t w;
    w.num_polygons = num_polygons;
    return w;
}

windstate_t intersect_add(windstate_t left, fillstyle_t*edge, segment_dir_t dir, int master)
{
    assert(master < left.num_polygons);

    left.wind_nr ^= 1<<master;
    if(left.wind_nr == (1<<left.num_polygons)-1)
        left.is_filled = 1;
    return left;
}

fillstyle_t* intersect_diff(windstate_t*left, windstate_t*right)
{
    if(left->is_filled==right->is_filled)
        return 0;
    else
        return &fillstyle_default;
}

windrule_t windrule_intersect = {
    start: intersect_start,
    add: intersect_add,
    diff: intersect_diff,
};

// -------------------- union ----------------------

windstate_t union_start(int num_polygons)
{
    return windstate_nonfilled;
}

windstate_t union_add(windstate_t left, fillstyle_t*edge, segment_dir_t dir, int master)
{
    assert(master<sizeof(left.wind_nr)*8); //up to 32/64 polygons max
    left.wind_nr ^= 1<<master;
    if(left.wind_nr!=0)
        left.is_filled = 1;
    return left;
}

fillstyle_t* union_diff(windstate_t*left, windstate_t*right)
{
    if(left->is_filled==right->is_filled)
        return 0;
    else
        return &fillstyle_default;
}

windrule_t windrule_union = {
    start: union_start,
    add: union_add,
    diff: union_diff,
};


/* 
 } else if (rule == WIND_NONZERO) {
     fill = wind!=0;
 } else if (rule == WIND_ODDEVEN) {
     fill = wind&1;
 } else { // rule == WIND_POSITIVE
     fill = wind>=1;
 }
 */