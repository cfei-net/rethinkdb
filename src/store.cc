#include "store.hpp"

struct mutation_get_key_functor_t : public boost::static_visitor<store_key_t> {
    store_key_t operator()(const get_cas_mutation_t &m) { return m.key; }
    store_key_t operator()(const sarc_mutation_t &m) { return m.key; }
    store_key_t operator()(const delete_mutation_t &m) { return m.key; }
    store_key_t operator()(const incr_decr_mutation_t &m) { return m.key; }
    store_key_t operator()(const append_prepend_mutation_t &m) { return m.key; }
};

store_key_t mutation_t::get_key() const {
    mutation_get_key_functor_t functor;
    return boost::apply_visitor(functor, mutation);
}

get_result_t set_store_interface_t::get_cas(sequence_group_t *seq_group, const store_key_t &key, order_token_t token) {
    get_cas_mutation_t mut;
    mut.key = key;
    return boost::get<get_result_t>(change(seq_group, mut, token).result);
}

set_result_t set_store_interface_t::sarc(sequence_group_t *seq_group, const store_key_t &key, unique_ptr_t<data_provider_t> data, mcflags_t flags, exptime_t exptime, add_policy_t add_policy, replace_policy_t replace_policy, cas_t old_cas, order_token_t token) {
    sarc_mutation_t mut;
    mut.key = key;
    mut.data = data;
    mut.flags = flags;
    mut.exptime = exptime;
    mut.add_policy = add_policy;
    mut.replace_policy = replace_policy;
    mut.old_cas = old_cas;
    return boost::get<set_result_t>(change(seq_group, mut, token).result);
}

incr_decr_result_t set_store_interface_t::incr_decr(sequence_group_t *seq_group, incr_decr_kind_t kind, const store_key_t &key, uint64_t amount, order_token_t token) {
    incr_decr_mutation_t mut;
    mut.kind = kind;
    mut.key = key;
    mut.amount = amount;
    return boost::get<incr_decr_result_t>(change(seq_group, mut, token).result);
}

append_prepend_result_t set_store_interface_t::append_prepend(sequence_group_t *seq_group, append_prepend_kind_t kind, const store_key_t &key, unique_ptr_t<data_provider_t> data, order_token_t token) {
    append_prepend_mutation_t mut;
    mut.kind = kind;
    mut.key = key;
    mut.data = data;
    return boost::get<append_prepend_result_t>(change(seq_group, mut, token).result);
}

delete_result_t set_store_interface_t::delete_key(sequence_group_t *seq_group, const store_key_t &key, order_token_t token, bool dont_put) {
    delete_mutation_t mut;
    mut.key = key;
    mut.dont_put_in_delete_queue = dont_put;
    return boost::get<delete_result_t>(change(seq_group, mut, token).result);
}

timestamping_set_store_interface_t::timestamping_set_store_interface_t(set_store_t *_target)
    : target(_target), cas_counter(0), timestamp(repli_timestamp_t::distant_past) { }

mutation_result_t timestamping_set_store_interface_t::change(sequence_group_t *seq_group, const mutation_t &mutation, order_token_t token) {
    assert_thread();
    return target->change(seq_group, mutation, make_castime(), token);
}

castime_t timestamping_set_store_interface_t::make_castime() {
    assert_thread();
    /* The cas-value includes the current time and a counter. The time is so that we don't assign
    the same CAS twice across multiple runs of the database. The counter is so that we don't assign
    the same CAS twice to two requests received in the same second. */
    cas_t cas = (uint64_t(timestamp.time) << 32) ^ uint64_t(++cas_counter);

    return castime_t(cas, timestamp);
}

void timestamping_set_store_interface_t::set_timestamp(repli_timestamp_t ts) {
    assert_thread();
    //    rassert(timestamp.time == 0 || timestamp < ts, "timestamp = %u, ts = %u", timestamp.time, ts.time);
    timestamp = std::max(timestamp, ts);
}
