#ifndef HALIDE_SCHEDULE_H
#define HALIDE_SCHEDULE_H

/** \file
 * Defines the internal representation of the schedule for a function 
 */

#include "IR.h"
#include <string>
#include <vector>

namespace Halide {
namespace Internal {

/** Information for loop partitioning from the .partition schedule.
 * It is stored in the Dim portion of the Schedule, and later into the For loops. */
struct PartitionInfo {
    /** One option is for the user to partition the main loop manually.
     * Specify an Interval for the loop.  The bounds can be expressions.
     * If not used, the expressions will be undefined. */
    Interval interval;
    /** Boolean options translate to tristate variables internally because they can
     * be undefined. */
    enum TriState { Undefined, No, Yes };
    /** Record the auto_partition option for this variable. */
    TriState auto_partition;
    
    PartitionInfo(bool do_partition) { interval = Interval(Expr(), Expr()); auto_partition = do_partition ? Yes : No; }
    PartitionInfo(Interval _interval) { interval = _interval; auto_partition = Undefined; }
};
        

/** A schedule for a halide function, which defines where, when, and
 * how it should be evaluated. */
struct Schedule {
    /** A reference to a site in a Halide statement at the top of the
     * body of a particular for loop. Evaluating a region of a halide
     * function is done by generating a loop nest that spans its
     * dimensions. We schedule the inputs to that function by
     * recursively injecting realizations for them at particular sites
     * in this loop nest. A LoopLevel identifies such a site. */
    struct LoopLevel {
        std::string func, var;

        /** Identify the loop nest corresponding to some dimension of some function */
        LoopLevel(std::string f, std::string v) : func(f), var(v) {}

        /** Construct an empty LoopLevel, which is interpreted as
         * 'inline'. This is a special LoopLevel value that implies
         * that a function should be inlined away */
        LoopLevel() {} 
        
        /** Test if a loop level corresponds to inlining the function */
        bool is_inline() const {return var.empty();}

        /** root is a special LoopLevel value which represents the
         * location outside of all for loops */
        static LoopLevel root() {
            return LoopLevel("", "<root>");
        }
        /** Test if a loop level is 'root', which describes the site
         * outside of all for loops */
        bool is_root() const {return var == "<root>";}

        /** Compare this loop level against the variable name of a for
         * loop, to see if this loop level refers to the site
         * immediately inside this loop. */
        bool match(const std::string &loop) const {
            return starts_with(loop, func + ".") && ends_with(loop, "." + var);
        }

    };

    /** At what sites should we inject the allocation and the
     * computation of this function? The store_level must be outside
     * of or equal to the compute_level. If the compute_level is
     * inline, the store_level is meaningless. See \ref Func::store_at
     * and \ref Func::compute_at */
    // @{
    LoopLevel store_level, compute_level;
    // @}

    struct Split {
        std::string old_var, outer, inner;
        Expr factor;
    };
    /** The traversal of the domain of a function can have some of its
     * dimensions split into sub-dimensions. See 
     * \ref ScheduleHandle::split */
    std::vector<Split> splits;
    
    struct Dim {
        std::string var;
        For::ForType for_type;
        PartitionInfo partition; //LH
    };
    /** The list and ordering of dimensions used to evaluate this
     * function, after all splits have taken place. The first
     * dimension in the vector corresponds to the innermost for loop,
     * and the last is the outermost. Also specifies what type of for
     * loop to use for each dimension. Does not specify the bounds on
     * each dimension. These get inferred from how the function is
     * used, what the splits are, and any optional bounds in the list below. */
    std::vector<Dim> dims;
    
    /** Record the auto partition option for this function.
     * Note that individual variable partition information overrides auto_partition 
     * for the entire function. */
    PartitionInfo::TriState auto_partition;
    
    /** Record the auto partition all option. */
    PartitionInfo::TriState auto_partition_all;
    
    /** The list and order of dimensions used to store this
     * function. The first dimension in the vector corresponds to the
     * innermost dimension for storage (i.e. which dimension is
     * tightly packed in memory) */
    std::vector<std::string> storage_dims;

    struct Bound {
        std::string var;
        Expr min, extent;
    };
    /** You may explicitly bound some of the dimensions of a
     * function. See \ref ScheduleHandle::bound */
    std::vector<Bound> bounds;
};

}
}

#endif
