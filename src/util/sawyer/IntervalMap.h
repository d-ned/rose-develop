#ifndef Sawyer_IntervalMap_H
#define Sawyer_IntervalMap_H

#include <sawyer/Map.h>
#include <sawyer/Optional.h>

#include <boost/cstdint.hpp>
#include <sawyer/Assert.h>

namespace Sawyer {
namespace Container {

/** Policy indicating how values are merged and split.
 *
 *  Adjacent nodes of an IntervalMap can be joined together provided their values can also be joined together.  This joining is
 *  a key feature of an IntervalMap since it can significantly reduce the number nodes required in the underlying map. */
template<typename I, typename T>
class MergePolicy {
public:
    typedef I Interval;
    typedef T Value;

    /** Merge two values if possible.
     *
     *  The @p rightValue is merged into the @p leftValue if possible, or this method returns false without changing either
     *  value.  After a successful merge, the @p rightValue will be removed from the IntervalMap and its destructor called. */
    bool merge(const Interval &leftInterval, Value &leftValue, const Interval &rightInterval, Value &rightValue) {
        return leftValue == rightValue;
    }

    /** Split one value into two values.
     *
     *  The IntervalMap calls this method when the @p interval is being split into two smaller, adjacent intervals. The @p
     *  splitPoint argument is the split point and becomes the least value of the right interval. The @p value argument is
     *  modified in place to become the left value, and the right value is returned. This method is only invoked when the
     *  result would be two non-empty intervals. */
    Value split(const Interval &interval, Value &value, const typename Interval::Value &splitPoint) { return value; }

    /** Discard the right part of a value.
     *
     *  This method is the same as @ref split except the right part of the resulting value is discarded.  This is sometimes
     *  more efficient than calling @ref split and then destroying the return value. */
    void truncate(const Interval &interval, Value &value, const typename Interval::Value &splitPoint) {}
};

/** An associative container whose keys are non-overlapping intervals.
 *
 *  This container is somewhat like an STL <code>std::map</code> in that it stores key/value pairs.  However, it is optimized
 *  for the case when many consecutive keys are the same or related.  The values may be any type; the keys are any interval
 *  type that follows the API and semantics of Sawyer::Container::Interval, namely a closed interval with members
 *  <code>least</code> and <code>greatest</code> demarcating the inclusive end points, and a few other methods.
 *
 *  The key/value pair nodes that are stored in this container are managed by the container, automatically joining adjacent
 *  nodes when they are inserted, if possible and permitted, and automatically spliting nodes if necessary when something is
 *  erased. For the most part, the user can think of this container as associating scalar keys with values, and almost forget
 *  that the container uses intervals as an optimization.
 *
 *  When two neighboring keys/value nodes are inserted, the container will possibly join them into a single key/value node.
 *  Normally, the merging of two  nodes happens if the two values are equal, but this can be influenced by a policy class
 *  provided as an argument of the container's constructor. See @ref MergePolicy for details.  Similarly, when part of an
 *  interval is erased, the container might need to split the affected node into two nodes, which is also handled by the
 *  policy.
 *
 *  The following examples demonstrates some aspects of the interface:
 *
 * @code
 *  typedef Sawyer::Container::interval<unsigned> Interval; // integral types work best
 *  class Stats {...} stats1=..., stats2=...; // needs at least a copy c'tor, assignment, and equality predicate.
 *  typedef IntervalMap<Interval, Stats> Map;
 *  Map map;
 *  map.insert(Interval(1,5), stats1);
 *  map.insert(6, stats2);
 * @endcode
 *
 *  If the policy allows the two "stats" objects to be merged (the default policy allows them to merge only if they are equal),
 *  then the container will end up having one node, the pair ([1,6], merge(stats1,stats2)), otherwise it will have two nodes.
 *
 * @code
 *  map.erase(Interval(2,3));
 * @endcode
 *
 *  Erasing keys 2 and 3 causes the affected node to split into two discontiguous nodes and a new copy of the node's
 *  value. Assuming we started with the two nodes { ([1,5], stats1), (6, stats2) }, then after erasing [2,3] the container will
 *  hold three nodes: { (1, stats1), ([4,5], stats1), (6, stats2) }.
 *
 *  Iteration over the container returns references to the nodes as @ref Node object that has <code>key</code> and
 *  <code>value</code> methods to access the interval and user-defined value parts of each storage node.  For example, here's
 *  one way to print the contents of the container, assuming the interval itself doesn't already have a printing function:
 *
 * @code
 *  std::cout <<"{";
 *  for (Map::ConstNodeIterator iter=map.nodes().begin(); iter!=map.nodes().end(); ++iter) {
 *      const Interval &interval = iter->key();
 *      const Stats &stats = iter->value();
 *      std::cout <<" (";
 *      if (singleton(interval))
 *          std::cout <<interval.least() <<", ";
 *      } else {
 *          std::cout <<"[" <<interval.least() <<"," <<interval.greatest() <<"], ";
 *      }
 *      std::cout <<stats <<")";
 *  }
 *  std::cout <<" }";
 * @endcode
 *
 *  Here's another way:
 *
 * @code
 *  BOOST_FOREACH (const Map::Node &node, map.nodes()) {
 *      const Interval &interval = node.key();
 *      const Stats &stats = node.value();
 *      ...
 *  }
 * @endcode
 *
 *  Besides <code>nodes()</code>, there's also <code>values()</code> and <code>keys()</code> that return bidirectional
 *  iterators over the user-defined values or the keys when dereferenced.
 *
 *  This class uses CamelCase for all its methods and inner types in conformance with the naming convention for the rest of the
 *  library. This includes iterator names (we don't use <code>iterator</code>, <code>const_iterator</code>, etc). */
template<typename I, typename T, class Policy = MergePolicy<I, T> >
class IntervalMap {
public:
    typedef I Interval;
    typedef T Value;

private:
    // Nodes of the underlying map are sorted by their last value so that we can use that map's lowerBound method to find the
    // range to which a scalar key might belong.  Since the intervals in the map are non-overlapping, sorting by greatest values
    // has the same effect as sorting by least values.
    struct IntervalCompare {
        bool operator()(const Interval &a, const Interval &b) const {
            return a.greatest() < b.greatest();
        }
    };

    typedef std::pair<Interval, Interval> IntervalPair;

public:
    /** Type of the underlying map. */
    typedef Container::Map<Interval, Value, IntervalCompare> Map;

    /** Storage node.
     *
     *  An Interval/Value pair with methods <code>key</code> and <code>value</code> for accessing the interval and its
     *  associated value. */
    typedef typename Map::Node Node;

    /** Iterator type.
     *
     *  This iterator visits the nodes of the container. Each node is an interval/value pair.
     * @{ */
    typedef typename Map::NodeIterator NodeIterator;
    typedef typename Map::ConstNodeIterator ConstNodeIterator;
    typedef typename Map::ConstKeyIterator ConstKeyIterator;
    typedef typename Map::ValueIterator ValueIterator;
    typedef typename Map::ConstValueIterator ConstValueIterator;
    /** @} */

private:
    Map map_;
    Policy policy_;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Constructors
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:
    /** Default constructor.
     *
     *  Creates an empty container. */
    IntervalMap() {}

    /** Copy constructor.
     *
     *  Initialize this container by copying all nodes from the @p other container.  This constructor has <em>O(n)</em>
     *  complexity, where <em>n</em> is the number of nodes in the container. */
    template<class Interval2, class T2, class Policy2>
    IntervalMap(const IntervalMap<Interval2, T2, Policy2> &other) {
        typedef typename IntervalMap<Interval2, T2, Policy2>::ConstNodeIterator OtherIterator;
        for (OtherIterator otherIter=other.nodes().begin(); other!=other.nodes().end(); ++other)
            insert(Interval(otherIter->key()), Value(otherIter->value()));
    }

    /** Assignment operator.
     *
     *  Makes this container look like the @p other container by clearing this container and then copying all nodes from the
     *  other container. */
    template<class Interval2, class T2, class Policy2>
    IntervalMap& operator=(const IntervalMap<Interval2, T2, Policy2> &other) {
        clear();
        typedef typename IntervalMap<Interval2, T2, Policy2>::ConstNodeIterator OtherIterator;
        for (OtherIterator otherIter=other.nodes().begin(); other!=other.nodes().end(); ++other)
            insert(Interval(otherIter->key()), Value(otherIter->value()));
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Searching
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:

    /** Iterators for traversing nodes.
     *
     *  Returns a range of iterators that traverse storage nodes (key/value pairs) for all nodes of this container.  The nodes
     *  are traversed in key order.
     *
     * @{ */
    boost::iterator_range<NodeIterator> nodes() { return map_.nodes(); }
    boost::iterator_range<ConstNodeIterator> nodes() const { return map_.nodes(); }
    /** @} */

    /** Iterators for traversing keys.
     *
     *  Returns a range of iteratores that traverse all keys (non-overlapping intervals) of this container according to the
     *  order of the intervals. */
    boost::iterator_range<ConstKeyIterator> keys() const { return map_.keys(); }

    /** Iterators for traversing values.
     *
     *  Returns a range of iterators that traverse the values (user-defined type) of this container.  The values are traversed
     *  in the order of their associated keys.
     *
     * @{ */
    boost::iterator_range<ValueIterator> values() { return map_.values(); }
    boost::iterator_range<ConstValueIterator> values() const { return map_.values(); }
    /** @} */

    /** Find the first node whose interval ends at or above the specified scalar key.
     *
     *  Returns an iterator to the node, or the end iterator if no such node exists.
     *
     * @{ */
    NodeIterator lowerBound(const typename Interval::Value &scalar) {
        return map_.lowerBound(Interval(scalar));
    }
    ConstNodeIterator lowerBound(const typename Interval::Value &scalar) const {
        return map_.lowerBound(Interval(scalar));
    }
    /** @} */

    /** Find the last node whose interval starts at or below the specified scalar key.
     *
     *  Returns an iterator to the node, or the end iterator if no such node exists.
     *
     * @{ */
    NodeIterator findPrior(const typename Interval::Value &scalar) {
        typedef NodeIterator Iter;
        if (isEmpty())
            return nodes().end();
        Iter lb = lowerBound(scalar);
        if (lb!=nodes().end() && lb->key().least() <= scalar)
            return lb;
        if (lb==nodes().begin())
            return nodes().end();
        return --lb;
    }
    ConstNodeIterator findPrior(const typename Interval::Value &scalar) const {
        typedef ConstNodeIterator Iter;
        if (isEmpty())
            return nodes().end();
        Iter lb = lowerBound(scalar);
        if (lb!=nodes().end() && lb->key().least() <= scalar)
            return lb;
        if (lb==nodes().begin())
            return nodes().end();
        return --lb;
    }
    /** @} */

    /** Find the node containing the specified scalar key.
     *
     *  Returns an iterator to the matching node, or the end iterator if no such node exists.
     *
     *  @{ */
    NodeIterator find(const typename Interval::Value &scalar) {
        typedef NodeIterator Iter;
        Iter found = lowerBound(scalar);
        if (found==nodes().end() || scalar < found->key().least())
            return nodes().end();
        return found;
    }
    ConstNodeIterator find(const typename Interval::Value &scalar) const {
        typedef ConstNodeIterator Iter;
        Iter found = lowerBound(scalar);
        if (found==nodes().end() || scalar < found->key().least())
            return nodes().end();
        return found;
    }
    /** @} */

    /** Find first interval that overlaps with the specified interval.
     *
     *  Returns an iterator to the matching node, or the end iterator if no such node exists.
     *
     * @{ */
    NodeIterator findFirstOverlap(const Interval &interval) {
        NodeIterator lb = lowerBound(interval.least());
        return lb!=nodes().end() && interval.isOverlapping(lb->key()) ? lb : nodes().end();
    }
    ConstNodeIterator findFirstOverlap(const Interval &interval) const {
        ConstNodeIterator lb = lowerBound(interval.least());
        return lb!=nodes().end() && interval.isOverlapping(lb->key()) ? lb : nodes().end();
    }
    /** @} */

    /** Find first interval that overlaps with any in another container.
     *
     *  The @p other container must use the same interval type, but may have different values and merge policies.  The search
     *  begins at the specified iterators and returns a pair of iterators pointing to the two nodes that overlap.  The first
     *  member of the pair is an iterator to this container, and the second is an iterator for the @p other container.  If no
     *  such nodes exist at or after the starting locations, then the return value will be a pair of end iterators for
     *  their respective containers.
     *
     * @{ */
    template<typename T2, class Policy2>
    std::pair<NodeIterator, typename IntervalMap<Interval, T2, Policy2>::ConstNodeIterator>
    findFirstOverlap(IntervalMap::NodeIterator thisIter, const IntervalMap<Interval, T2, Policy2> &other,
                     typename IntervalMap<Interval, T2, Policy2>::ConstNodeIterator otherIter) {
        while (thisIter!=nodes().end() && otherIter!=other.nodes().end()) {
            if (thisIter->key().isOverlapping(otherIter->key()))
                return std::make_pair(thisIter, otherIter);
            if (thisIter->key().greatest() < otherIter->key().greatest()) {
                ++thisIter;
            } else {
                ++otherIter;
            }
        }
        return std::make_pair(nodes().end(), other.nodes().end());
    }
    template<typename T2, class Policy2>
    std::pair<ConstNodeIterator, typename IntervalMap<Interval, T2, Policy2>::ConstNodeIterator>
    findFirstOverlap(IntervalMap::ConstNodeIterator thisIter, const IntervalMap<Interval, T2, Policy2> &other,
                     typename IntervalMap<Interval, T2, Policy2>::ConstNodeIterator otherIter) const {
        while (thisIter!=nodes().end() && otherIter!=other.nodes().end()) {
            if (thisIter->key().isOverlapping(otherIter->key()))
                return std::make_pair(thisIter, otherIter);
            if (thisIter->key().greatest() < otherIter->key().greatest()) {
                ++thisIter;
            } else {
                ++otherIter;
            }
        }
        return std::make_pair(nodes().end(), other.nodes().end());
    }
    /** @} */

    /** Find the first fit node at or after a starting point.
     *
     *  Finds the first node of contiguous values beginning at or after the specified starting iterator, @p start, and which is
     *  at least as large as the desired @p size.  If there are no such nodes then the end iterator is returned.
     *
     *  Caveat emptor: The @p size argument has the name type as the interval end points. If the end points have a signed type,
     *  then it is entirely likely that the size will overflow.  In fact, it is also possible that unsigned sizes overflow
     *  since, for example, an 8-bit unsigned size cannot hold the size of an interval representing the entire 8-bit space.
     *  Therefore, use this method with care.
     *
     * @{ */
    NodeIterator firstFit(const typename Interval::Value &size, NodeIterator start) {
        typedef NodeIterator Iter;
        for (Iter iter=start; iter!=nodes().end(); ++iter) {
            if (isLarge(iter->key(), size))
                return iter;
        }
        return nodes().end();
    }
    ConstNodeIterator firstFit(const typename Interval::Value &size, ConstNodeIterator start) const {
        typedef ConstNodeIterator Iter;
        for (Iter iter=start; iter!=nodes().end(); ++iter) {
            if (isLarge(iter->key(), size))
                return iter;
        }
        return nodes().end();
    }
    /** @} */

    /** Find the best fit node at or after a starting point.
     *
     *  Finds a node of contiguous values beginning at or after the specified starting iterator, @p start, and which is at
     *  least as large as the desired @p size.  If there is more than one such node, then the first smallest such node is
     *  returned.  If there are no such nodes then the end iterator is returned.
     *
     *  Caveat emptor: The @p size argument has the name type as the interval end points. If the end points have a signed type,
     *  then it is entirely likely that the size will overflow.  In fact, it is also possible that unsigned sizes overflow
     *  since, for example, an 8-bit unsigned size cannot hold the size of an interval representing the entire 8-bit space.
     *  Therefore, use this method with care.
     *
     * @{ */
    NodeIterator bestFit(const typename Interval::Value &size, NodeIterator start) {
        typedef NodeIterator Iter;
        Iter best = nodes().end();
        for (Iter iter=start; iter!=nodes().end(); ++iter) {
            if (iter->key().size()==size && size!=0)
                return iter;
            if (iter->key().size() > size && (best==nodes().end() || iter->key().size() < best->key().size()))
                best = iter;
        }
        return best;
    }
    ConstNodeIterator bestFit(const typename Interval::Value &size, ConstNodeIterator start) const {
        typedef ConstNodeIterator Iter;
        Iter best = nodes().end();
        for (Iter iter=start; iter!=nodes().end(); ++iter) {
            if (iter->key().size()==size && size!=0)
                return iter;
            if (iter->key().size() > size && (best==nodes().end() || iter->key().size() < best->key().size()))
                best = iter;
        }
        return best;
    }
    /** @} */


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Accessors
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:
    /** Returns a reference to an existing value.
     *
     *  Returns a reference to the value at the node with the specified @p scalar.  Unlike <code>std::map</code>, this
     *  container does not instantiate a new value if the @p scalar key is not in the map's domain.  In other words, the
     *  array operator for this class is more like an array operator on arrays or vectors--such objects are not automatically
     *  extended if dereferenced with an operand that is outside the domain.
     *
     *  If the @p scalar is not part of this map's domain then an <code>std:domain_error</code> is thrown.
     *
     *  @{ */
    Value& operator[](const typename Interval::Value &scalar) {
        NodeIterator found = find(scalar);
        if (found==nodes().end())
            throw std::domain_error("key lookup failure; key is not in map domain");
        return found->value();
    }
    const Value& operator[](const typename Interval::Value &scalar) const {
        ConstNodeIterator found = find(scalar);
        if (found==nodes().end())
            throw std::domain_error("key lookup failure; key is not in map domain");
        return found->value();
    }
    // FIXME[Robb Matzke 2014-06-05]: need get() for consistency with class Map
    /** @} */

    /** Lookup and return a value or nothing.
     *
     *  Looks up the node with the specified @p scalar key and returns either a copy of its value, or nothing. This method
     *  executes in logorithmic time.
     *
     *  Here's an example of one convenient way to use this:
     *
     * @code
     *  IntervalMap<AddressInterval, FileInfo> files;
     *  ...
     *  if (Optional<FileInfo> fileInfo = files.getOptional(address))
     *      std::cout <<"file info for " <<address <<" is " <<*fileInfo <<"\n";
     * @endcode */
    Optional<Value> getOptional(const typename Interval::Value &scalar) const {
        ConstNodeIterator found = find(scalar);
        return found == nodes().end() ? Optional<Value>() : Optional<Value>(found->value());
    }
    
    /** Lookup and return a value or something else.
     *
     *  This is similar to the @ref getOptional method, except a default can be provided.  If a node with the specified @p
     *  scalar key is present in this container, then a reference to that node's value is returned, otherwise the (reference
     *  to) supplied default is returned.
     *
     * @{ */
    Value& getOrElse(const typename Interval::Value &scalar, Value &dflt) {
        NodeIterator found = find(scalar);
        return found == nodes().end() ? dflt : found->value();
    }
    const Value& getOrElse(const typename Interval::Value &scalar, const Value &dflt) const {
        ConstNodeIterator found = find(scalar);
        return found == nodes().end() ? dflt : found->value();
    }
    /** @} */

    /** Lookup and return a value or a default.
     *
     *  This is similar to the @ref getOrElse method except when the @p scalar key is not present in the map, a reference to a
     *  const, default-constructed value is returned. */
    const Value& getOrDefault(const typename Interval::Value &scalar) const {
        static const Value dflt;
        ConstNodeIterator found = find(scalar);
        return found==nodes().end() ? dflt : found->value();
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Capacity
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:

    /** Determine if the container is empty.
     *
     *  Returns true if this container has no nodes. */
    bool isEmpty() const {
        return map_.isEmpty();
    }

    /** Number of nodes in the container.
     *
     *  Each node is a pair consisting of an interval and a value.  The container normally merges two juxtaposed intervals if
     *  their values can be combined. */
    size_t nIntervals() const {
        return map_.size();
    }

    /** Returns the number of values represented by this container.
     *
     *  The number of values in a container is the sum of the widths of all the nodes. */
    typename Interval::Value size() const {
        typename Interval::Value sum = 0;
        for (ConstKeyIterator iter=keys().begin(); iter!=keys().end(); ++iter)
            sum += iter->size();
        return sum;
    }

    /** Returns the minimum scalar key. */
    typename Interval::Value least() const {
        ASSERT_forbid(isEmpty());
        return map_.keys().begin()->least();
    }

    /** Returns the maximum scalar key. */
    typename Interval::Value greatest() const {
        ASSERT_forbid(isEmpty());
        ConstKeyIterator last = map_.keys().end(); --last;
        return last->greatest();
    }

    /** Returns the limited-minimum scalar key.
     *
     *  Returns the minimum scalar key that exists in the map and which is greater than or equal to @p lowerLimit.  If no such
     *  value exists then nothing is returned. */
    Optional<typename Interval::Value> least(typename Interval::Value lowerLimit) const {
        ConstNodeIterator found = lowerBound(lowerLimit); // first node ending at or after lowerLimit
        if (found==nodes().end())
            return Nothing();
        return std::max(lowerLimit, found->key().least());
    }

    /** Returns the limited-maximum scalar key.
     *
     *  Returns the maximum scalar key that exists in the map and which is less than or equal to @p upperLimit.  If no such
     *  value exists then nothing is returned. */
    Optional<typename Interval::Value> greatest(typename Interval::Value upperLimit) const {
        ConstNodeIterator found = findPrior(upperLimit); // last node beginning at or before upperLimit
        if (found==nodes().end())
            return Nothing();
        return std::min(upperLimit, found->key().greatest());
    }

    /** Returns the limited-minimum unmapped scalar key.
     *
     *  Returns the lowest unmapped scalar key equal to or greater than the @p lowerLimit.  If no such value exists then
     *  nothing is returned. */
    Optional<typename Interval::Value> leastUnmapped(typename Interval::Value lowerLimit) const {
        for (ConstNodeIterator iter = lowerBound(lowerLimit); iter!=nodes().end(); ++iter) {
            if (lowerLimit < iter->key().least())
                return lowerLimit;
            lowerLimit = iter->key().greatest() + 1;
            if (lowerLimit < iter->key().greatest())
                return Nothing();                       // overflow
        }
        return lowerLimit;
    }

    /** Returns the limited-maximum unmapped scalar key.
     *
     *  Returns the maximum unmapped scalar key equal to or less than the @p upperLimit.  If no such value exists then nothing
     *  is returned. */
    Optional<typename Interval::Value> greatestUnmapped(typename Interval::Value upperLimit) const {
        for (ConstNodeIterator iter = findPrior(upperLimit); iter!=nodes().end(); --iter) {
            if (upperLimit > iter->key().greatest())
                return upperLimit;
            upperLimit = iter->key().least()- 1;
            if (upperLimit > iter->key().least())
                return Nothing();                       // overflow
            if (iter==nodes().begin())
                break;
        }
        return upperLimit;
    }
    
    /** Returns the range of values in this map. */
    Interval hull() const {
        return isEmpty() ? Interval() : Interval::hull(least(), greatest());
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Mutators
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:

    /** Empties the container. */
    void clear() {
        map_.clear();
    }

    /** Erase the specified interval. */
    void erase(const Interval &erasure) {
        if (erasure.isEmpty())
            return;

        // Find what needs to be removed, and create a list of things to insert, but delay actual removing until after
        // the loop since Map::erase doesn't return a next iterator.
        Map insertions;                                 // what needs to be inserted back in
        NodeIterator eraseBegin = nodes().end();
        NodeIterator iter;
        for (iter=lowerBound(erasure.least()); iter!=nodes().end() && !erasure.isLeftOf(iter->key()); ++iter) {
            Interval foundInterval = iter->key();
            Value &v = iter->value();
            if (erasure.isContaining(foundInterval)) {
                // erase entire found interval
                if (eraseBegin==nodes().end())
                    eraseBegin = iter;
            } else if (erasure.least()>foundInterval.least()&& erasure.greatest()<foundInterval.greatest()) {
                // erase the middle of the node, leaving a left and a right portion
                ASSERT_require(eraseBegin==nodes().end());
                eraseBegin = iter;
                IntervalPair rt = splitInterval(foundInterval, erasure.greatest()+1);
                Value rightValue = policy_.split(foundInterval, v /*in,out*/, rt.second.least());
                insertions.insert(rt.second, rightValue);
                IntervalPair lt = splitInterval(rt.first, erasure.least());
                policy_.truncate(rt.first, v /*in,out*/, erasure.least());
                insertions.insert(lt.first, v);
            } else if (erasure.least() > foundInterval.least()) {
                // erase the right part of the node
                ASSERT_require(eraseBegin==nodes().end());
                eraseBegin = iter;
                IntervalPair halves = splitInterval(foundInterval, erasure.least());
                policy_.truncate(foundInterval, v /*in,out*/, erasure.least());
                insertions.insert(halves.first, v);
            } else if (erasure.greatest() < foundInterval.greatest()) {
                // erase the left part of the node
                if (eraseBegin==nodes().end())
                    eraseBegin = iter;
                IntervalPair halves = splitInterval(foundInterval, erasure.greatest()+1);
                Value rightValue = policy_.split(foundInterval, v /*in,out*/, halves.second.least());
                insertions.insert(halves.second, rightValue);
            }
        }

        // Do the actual erasing and insert the new stuff, which is easy now because we know it doesn't overlap with anything.
        if (eraseBegin!=nodes().end())
            map_.eraseAtMultiple(eraseBegin, iter);
        map_.insertMultiple(insertions.nodes());
    }

    /** Erase intervals specified in another IntervalMap
     *
     *  Every interval in @p other is erased from this container. */
    template<typename T2, class Policy2>
    void eraseMultiple(const IntervalMap<Interval, T2, Policy2> &other) {
        ASSERT_forbid2((const void*)&other != (const void*)this, "use clear() instead");
        typedef typename IntervalMap<Interval, T2, Policy2>::ConstNodeIterator OtherIter;
        for (OtherIter oi=other.nodes().begin(); oi!=other.nodes().end(); ++oi)
            erase(oi->key());
    }

    /** Insert a key/value pair.
     *
     *  If @p makeHole is true then the interval being inserted is first erased; otherwise the insertion happens only if none
     *  of the interval being inserted already exists in the container.  */
    void insert(Interval key, Value value, bool makeHole=true) {
        if (key.isEmpty())
            return;
        if (makeHole) {
            erase(key);
        } else {
            NodeIterator found = lowerBound(key.least());
            if (found!=nodes().end() && key.isOverlapping(found->key()))
                return;
        }

        // Attempt to merge with a left-adjoining node
        if (key.least() - 1 < key.least()) {
            NodeIterator left = find(key.least() - 1);
            if (left!=nodes().end() &&
                left->key().greatest()+1==key.least() &&
                policy_.merge(left->key(), left->value(), key, value)) {
                key = Interval::hull(left->key().least(), key.greatest());
                std::swap(value, left->value());
                map_.eraseAt(left);
            }
        }

        // Attempt to merge with a right-adjoining node
        if (key.greatest() + 1 > key.greatest()) {
            NodeIterator right = find(key.greatest() + 1);
            if (right!=nodes().end() &&
                key.greatest()+1==right->key().least() &&
                policy_.merge(key, value, right->key(), right->value())) {
                key = Interval::hull(key.least(), right->key().greatest());
                map_.eraseAt(right);
            }
        }

        map_.insert(key, value);
    }

    /** Insert values from another container.
     *
     *  The values in the other container must be convertable to values of this container, and the intervals must be the same
     *  type. */
    template<typename T2, class Policy2>
    void insertMultiple(const IntervalMap<Interval, T2, Policy2> &other, bool makeHole=true) {
        ASSERT_forbid2((const void*)&other != (const void*)this, "cannot insert a container into itself");
        typedef typename IntervalMap<Interval, T2, Policy>::ConstNodeIterator OtherIter;
        for (OtherIter oi=other.nodes().begin(); oi!=other.nodes().end(); ++oi)
            insert(oi->key(), Value(oi->value()), makeHole);
    }

// FIXME[Robb Matzke 2014-04-13]
//    // Intersection
//    void intersect(const Interval&);
//    template<T2, Policy2>
//    void intersect(const IntervalMap<Interval, T2, Policy2> &other);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Predicates
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:
    bool isOverlapping(const Interval &interval) const {
        return findFirstOverlap(interval)!=nodes().end();
    }

    template<typename T2, class Policy2>
    bool isOverlapping(const IntervalMap<Interval, T2, Policy2> &other) const {
        return findFirstOverlap(other).first!=nodes().end();
    }

    bool isDistinct(const Interval &interval) const {
        return !isOverlapping(interval);
    }

    template<typename T2, class Policy2>
    bool isDistinct(const IntervalMap<Interval, T2, Policy2> &other) const {
        return !isOverlapping(other);
    }

    bool contains(Interval key) const {
        if (key.isEmpty())
            return true;
        ConstNodeIterator found = find(key.least());
        while (1) {
            if (found==nodes().end())
                return false;
            if (key.least() < found->key().least())
                return false;
            ASSERT_require(key.isOverlapping(found->key()));
            if (key.greatest() <= found->key().greatest())
                return true;
            key = splitInterval(key, found->key().greatest()+1).second;
            ++found;
        }
    }

//
//    template<T2, Policy2>
//    bool contains(const IntervalMap<Interval, T2, Policy2> &other) const;
//

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  Private support methods
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
    static IntervalPair splitInterval(const Interval &interval, const typename Interval::Value &splitPoint) {
        ASSERT_forbid(interval.isEmpty());
        ASSERT_require(splitPoint > interval.least() && splitPoint <= interval.greatest());

        Interval left = Interval::hull(interval.least(), splitPoint-1);
        Interval right = Interval::hull(splitPoint, interval.greatest());
        return IntervalPair(left, right);
    }

    // a more convenient way to check whether interval contains at least size items and still handle overflow
    static bool isLarge(const Interval &interval, boost::uint64_t size) {
        return !interval.isEmpty() && (interval.size()==0 || interval.size() >= size);
    }
};

} // namespace
} // namespace

#endif
