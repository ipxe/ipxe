/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * Peer-to-peer download abstraction
 *
 * We define a concept of a peer-to-peer file download operation as
 * comprising a fixed list of variable-length file segments that must
 * be downloaded and a variable set of peers from which we may attempt
 * to retrieve each segment.
 *
 * The major peer-to-peer operations are:
 *
 *   - The selection of the next peer from which to attempt the
 *     retrieval of a segment
 *
 *   - Measurement and estimation of various aspects of the retrieval
 *     attempt, to inform future selection decisions
 *
 *   - Maintenance of a discovered peer list
 *
 * The peer-to-peer download abstraction is modelled on the practical
 * operation of the PeerDist protocol, but is designed to be passive
 * and independent of the actual wire protocol.  It provides the peer
 * selection algorithm, but does not itself initiate or process any
 * network traffic.
 *
 * We assume that some kind of discovery mechanism exists to detect
 * peers, but that this operates at a much slower timescale than the
 * retrieval mechanism.  We expect to perform only a handful of
 * discoveries per thousand segments, and we assume that we have
 * imperfect information about which peers may hold each segment.
 *
 * (This maps well onto the way that PeerDist/BranchCache must be used
 * in practice: in theory a PeerDist client should perform discovery
 * for every segment, but doing so drops throughput by three orders of
 * magnitude and so clients must instead optimistically attempt to
 * retrieve segments from existing peers.)
 *
 *
 * PEER PROPERTIES
 * ---------------
 *
 * For each peer [i], we maintain the following properties:
 *
 *   - p[i] ("probability"): the estimated probability that a valid
 *     retrieval response for a previously unattempted segment from
 *     this peer will be a successful response
 *
 *   - l[i] ("latency"): the estimated elapsed time until the first
 *     byte of a valid retrieval response from this peer
 *
 *   - u[i] ("unit cost"): the estimated additional elapsed time per
 *     byte of segment data from the first byte to the last byte of a
 *     successful retrieval response from this peer
 *
 *   - v[i] ("variation"): the estimated absolute difference between
 *     the initially estimated total successful retrieval response
 *     time (calculated from l[i] and u[i]) and the measured total
 *     successful retrieval response time from this peer
 *
 *   - n[i] ("number"): the number of currently active retrieval
 *     attempts from this peer by this client
 *
 *   - f[i] ("failure"): the number of consecutive retrieval attempts
 *     from this peer that did not result in a valid retrieval
 *     response (noting that an unsuccessful response still counts as
 *     a valid response)
 *
 *   - w[i] ("weight"): the relative cost weighting of attempting
 *     retrieval from this peer
 *
 * The estimated values are updated from measured values using an
 * exponentially weighted moving average estimator.  For example, for
 * each retrieval attempt on a previously unattempted segment, the
 * probability estimate p[i] will be updated towards 1.0 if the
 * retrieval is successful or updated towards 0.0 if the retrieval is
 * unsuccessful.
 *
 * Some peer properties are used to support aspects of operation that
 * are not directly related to the peer selection algorithm.  For
 * example, peers may be forcibly evicted from the peer list after a
 * certain number of failures to obtain a valid retrieval response,
 * and the estimated variation may be used to calculate an appropriate
 * timeout for a retrieval attempt.
 *
 *
 * PEER GROUP PROPERTIES
 * ---------------------
 *
 * We define groups of peers that are expected to share similar
 * characteristics.  For example: peers discovered via multicast on a
 * local network may be in one group, with predefined (and potentially
 * remote) peers placed into a separate group.  For each peer group
 * [g], we maintain the following properties:
 *
 *   - p[g] ("probability"): the estimated probability that a valid
 *     retrieval response for a previously unattempted segment from a
 *     peer within this group will be a successful response
 *
 *   - r[g] ("retry"): the estimated fraction (0<=r[g]<=1) of a group
 *     member's probability p[i] that is retained after each valid but
 *     unsuccessful retrieval response for a particular segment from
 *     any peer within this group
 *
 *   - l[g] ("latency"): the estimated elapsed time until the first
 *     byte of a valid retrieval response from a peer within this
 *     group
 *
 *   - u[g] ("unit cost"): the estimated additional elapsed time per
 *     byte of segment data from the first byte to the last byte of a
 *     successful retrieval response from a peer within this group
 *
 *   - v[g] ("variation"): the estimated absolute difference between
 *     the initially estimated total successful retrieval response
 *     time (calculated from l[g] and u[g]) and the measured total
 *     successful retrieval response time from a peer within this
 *     group
 *
 *   - w[g] ("weight"): the policy-defined relative cost weighting of
 *     attempting retrieval from a peer within this group
 *
 *   - pT[g] ("probability threshold"): the policy-defined value of
 *     p[i] below which a peer becomes eligible for eviction in favour
 *     of newly discovered peers
 *
 *   - p0[g] ("initial probability"): the a priori value of p[g] before
 *     any measurements have been taken
 *
 *   - r0[g] ("initial retry"): the a priori value of r[g] before any
 *     measurements have been taken
 *
 *   - l0[g] ("initial latency"): the a priori value of l[g] before
 *     any measurements have been taken
 *
 *   - u0[g] ("initial unit cost"): the a priori value of u[g] before
 *     any measurements have been taken
 *
 *   - v0[g] ("initial variation"): the a priori value of v[g] before
 *     any measurements have been taken
 *
 * The set of peers may include some special-function peers, such as a
 * local filesystem cache (which will be extremely fast for any
 * segments that it happens to hold), or an origin server (which may
 * be extremely slow but is expected to hold all segments).  These are
 * modelled as peer groups with suitable characteristics (e.g. a high
 * cost weighting w[g] for an origin server).
 *
 * Note that p[g] is an estimate for the probability of getting a
 * successful response from an individual peer within the group, not
 * an estimate of the probability of getting a successful response
 * from the group as a whole.  (The latter depends also upon the
 * number of peers within the group and the extent to which the
 * subsets of cached segments are correlated between peers.)
 *
 * The probability threshold pT[g] affects only eligibility for
 * eviction in favour of newly discovered peers: it does not constrain
 * the forcible eviction of failed peers.
 *
 *
 * EXPLORATION MECHANISM
 * ---------------------
 *
 * Peers that are not selected for retrieval will not be measured and
 * their estimates will become stale.  We therefore drift each peer's
 * estimate weakly towards its corresponding group-level estimate, so
 * that the peer may eventually be reconsidered for selection within
 * the group.
 *
 * Similarly, peer groups that are not selected for retrieval will not
 * be measured and their estimates will become stale.  We therefore
 * drift each peer group's estimates weakly back towards the a priori
 * estimates for that group.
 *
 * Note that the group-level estimators are updated directly from
 * measurements (rather than calculating the group-level estimate as
 * the average of the group members' estimates), to avoid introducing
 * a positive feedback loop via the drift mechanism.
 *
 * The initial values for the estimates of a newly discovered peer may
 * be taken from its group's current estimates.  The various a priori
 * values for a peer group must be optimistic (e.g. assuming that
 * p0[g]=1.0) so that a retrieval from within the group will be
 * attempted and real measurements may be obtained.  A peer group that
 * is initialised with extremely pessimistic values (e.g. p0[g]=0.0)
 * would never be selected.
 *
 * Some properties (e.g. the retry probability fraction) are expected
 * to be measured extremely rarely for any individual peer.  To
 * counteract the low expected sample count for any individual peer,
 * these properties are estimated only at the group level.
 *
 *
 * PEER SELECTION ALGORITHM
 * ------------------------
 *
 * For each segment within the file, we select a peer and attempt
 * retrieval from that peer.  If the retrieval fails, then we continue
 * selecting and attempting peers until the retrieval of that segment
 * succeeds (or until all possible attempts have been exhausted).
 *
 * For a segment of length L bytes, we can estimate the time T[i] to
 * complete a retrieval attempt from peer [i] as:
 *
 *   T[i] = { l[i] + L*u[i]	if the retrieval is successful
 *          { l[i]		if the retrieval is not successful
 *
 * The latency for an unsuccessful retrieval may be lower than the
 * latency for a successful retrieval, since the former potentially
 * requires less processing by the peer.  We do not attempt to
 * estimate this difference, since we do not expect to obtain a large
 * enough number of measurements to obtain a high-quality estimate.
 *
 * We allow for the possibility that peers may report a false miss as
 * a crude load-balancing mechanism (or, equivalently, that peers may
 * obtain segments that they did not previously hold), by allowing for
 * up to a fixed number of retries from each peer.  To support this,
 * we must maintain a per-peer counter for each segment to track how
 * many times we have already attempted retrieval of that segment from
 * that peer.  To avoid the requirement to dynamically allocate
 * storage for each {segment, peer} combination, we place a fixed
 * upper limit on the total number of recorded peers, allocate each
 * peer a unique peer index within this range, and use a fixed-size
 * per-segment array to record the number of retrieval attempts made
 * for each peer index.  Define:
 *
 *   - a[i] ("attempt"): the attempt count for the current segment
 *     from peer [i] (with a[i]=0 being the first attempt)
 *
 * The probability q[i] that the next retrieval attempt from peer [i]
 * within peer group [g] will be successful is therefore:
 *
 *   q[i] = p[i] * r[g]^a[i]
 *
 * The expected time to complete a retrieval attempt from peer [i]
 * (whether successful or not) is then:
 *
 *   E[i] = l[i] + q[i]*L*u[i]
 *
 * Define the cost C[i] of a retrieval attempt to be the expected time
 * E[i] multiplied by the relative cost weighting w[i]:
 *
 *   C[i] = w[i]*E[i]
 *        = w[i]*(l[i] + q[i]*L*u[i])
 *
 * Consider an ordering strategy {i_1, i_2, ...} in which each peer
 * will be attempted in turn until the first attempt that succeeds in
 * retrieving the segment.  The expected cost of executing this
 * strategy is then:
 *
 *   C = sum{k}( C[i_k] * product{j<k}( (1-q[i_j]) ) )
 *
 * We can minimise this expected cost using essentially the argument
 * used to prove correctness of the standard bubble sort algorithm.
 * Consider two peers x and y that are currently ordered adjacently
 * within the strategy, and then consider the effect on the cost if
 * the order of these two peers were to be swapped.  Multiplication of
 * the (1-q[x])(1-q[y]) terms is commutative, and so all preceding and
 * all following terms within the summation would be unaffected by the
 * swap and may be disregarded for the purpose of comparison.  For the
 * two terms that remain, the product of all preceding terms within
 * the product is identical and may be similarly factored out and
 * disregarded.  We are therefore left to compare only:
 *
 *   C[x] + C[y]*(1-q[x])  <=>  C[y] + C[x]*(1-q[y])
 *
 *              C[x]*q[y]  <=>  C[y]*q[x]
 *
 *              C[x]/q[x]  <=>  C[y]/q[y]
 *
 * This gives us a peer score s[i]=C[i]/q[i] for each peer that we can
 * use as a sort key.  The overall expected cost is minimised if we
 * sort peers in ascending order of peer score s[i].
 *
 * We never need to construct the full sorted list.  At each decision
 * point, the question is simply which peer to attempt next.  The
 * minimal expected cost is obtained when peers are sorted in
 * ascending order of s[i], and so the best peer to try next is the
 * peer with the lowest peer score s[i].  Expanding and cancelling:
 *
 *   s[i] = C[i]/q[i]
 *        = w[i]*(l[i] + q[i]*L*u[i])/q[i]
 *        = w[i]*(l[i]/q[i] + L*u[i])
 *
 * Note that the expected cost function assumes independence between
 * retrieval attempts from different peers (i.e. that a failure on one
 * peer does not predict either failure or success on the next peer to
 * be tried).  This assumption may not be correct if there is a strong
 * correlation between the set of segments held by peers (e.g. if
 * peers share an underlying segment cache).  When some correlation
 * exists, the cost function becomes optimistic and the peer selection
 * algorithm degenerates to become a reasonable heuristic.
 *
 * Note also that the nominal full sorted list will change every time
 * we make a new decision.  For example: the estimated peer properties
 * may have changed due to new measurements, or the q[i] probability
 * estimate for a peer may have changed because a retrieval has
 * already been attempted.
 *
 *
 * RANDOMISATION
 * -------------
 *
 * We introduce some deliberate suboptimality to avoid clustering and
 * herding behaviours between multiple clients all concurrently
 * attempting the same download from the same set of available peers.
 *
 * Firstly, we choose to construct the individual peer cost weighting
 * w[i] by weakly randomising the policy-defined peer group cost
 * weighting w[g].  This results in a slight peer preference that
 * differs between clients but is consistent for each client.  In the
 * case of a set of identically performing peers, this acts to
 * distribute clients across multiple peers.
 *
 * Secondly, we do not always choose the best peer (i.e. the peer with
 * the lowest peer score).  Instead we choose from amongst the subset
 * of peers that have a score that falls inside a band within a fixed
 * multiple of the lowest peer score.  Where scores are widely varying
 * between peers (e.g. a local filesystem cache versus a LAN peer),
 * this will cause us to always choose the better scoring peer.  Where
 * scores are similar, this allows us to statistically balance load
 * amongst multiple comparable peers.  From the subset of peers within
 * the band, we choose the peer that has the lowest number n[i] of
 * concurrent connections (breaking ties by choosing the lowest peer
 * score within this subset of the subset).  This provides some load
 * balancing even when only one client is active, and decorrelates
 * this peer selection decision between different clients operating
 * with the same set of peers.
 *
 *
 * DISCOVERY
 * ---------
 *
 * Decisions on when to attempt peer discovery are left to the
 * consumer (e.g. the PeerDist protocol).  The peer group properties
 * such as p[g] (or the individual peer properties such as p[i]) may
 * be used to inform the decision on whether or not discovery is worth
 * the cost: if there is already a very high probability of
 * successfully retrieving segments from the existing peers then the
 * value of further discovery may be minimal.
 *
 * Note that the costs of discovery are primarily borne by other nodes
 * on the network: a multicast discovery probe may need to be received
 * and processed by every one of a large group of potential peers.
 * Clients should therefore aim to be conservative in performing
 * discovery.
 *
 * The maximum number of peers is fixed (as described above, to allow
 * for each segment to have a fixed-size attempt counter array), and
 * so the addition of a new peer may require an existing peer to be
 * evicted from the set so that its peer index may be reused.
 *
 * Discovery may operate with lower authenticity guarantees than
 * retrieval.  For example: it may be possible for any node on the
 * network to send a response to a discovery request, but possible
 * only for nodes in possession of a shared secret to send a (valid)
 * response to a retrieval request.  To guard against an eviction
 * attack on the peer list, we choose as eviction candidates only
 * peers that are within the same group as the newly discovered peer
 * and that have a current probability estimate p[i] that is below the
 * group's predefined probability threshold pT[g].  This ensures that
 * a rogue discovery response cannot directly cause useful peers to be
 * evicted.
 *
 * If this set of eviction candidates is non-empty, then we choose to
 * evict the peer with the lowest current probability estimate p[i].
 * If the set of eviction candidates is empty, then we ignore the
 * newly discovered peer.
 *
 * Some peer groups (e.g. those containing the special-function peers
 * that represent a local filesystem cache or a remote origin server)
 * may not be subject to either discovery or eviction, and will
 * instead contain a fixed list of peers created when the download is
 * instantiated.
 *
 */
