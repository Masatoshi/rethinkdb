// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef CLUSTERING_ADMINISTRATION_NAMESPACE_INTERFACE_REPOSITORY_HPP_
#define CLUSTERING_ADMINISTRATION_NAMESPACE_INTERFACE_REPOSITORY_HPP_

#include <map>

#include "clustering/administration/metadata.hpp"
#include "containers/clone_ptr.hpp"
#include "concurrency/auto_drainer.hpp"
#include "concurrency/one_per_thread.hpp"
#include "concurrency/promise.hpp"
#include "containers/incremental_lenses.hpp"

/* `namespace_repo_t` is responsible for constructing and caching
`cluster_namespace_interface_t` objects for all of the namespaces in the cluster
for a given protocol. Caching `cluster_namespace_interface_t` objects is
important because every time a new `cluster_namespace_interface_t` is created,
it must perform a handshake with every `master_t`, which means several network
round-trips. */

class mailbox_manager_t;
class namespace_interface_t;
class namespaces_directory_metadata_t;
class peer_id_t;
class rdb_context_t;
class signal_t;
class uuid_u;
template <class> class watchable_t;

class base_namespace_repo_t {
protected:
    struct namespace_cache_entry_t;

public:
    base_namespace_repo_t() { }
    virtual ~base_namespace_repo_t() { }

    class access_t {
    public:
        access_t();
        access_t(base_namespace_repo_t *parent, const uuid_u &ns_id,
                 signal_t *interruptor);
        access_t(const access_t &access);
        access_t &operator=(const access_t &access);

        namespace_interface_t *get_namespace_if();

    private:
        struct ref_handler_t {
        public:
            ref_handler_t();
            ~ref_handler_t();
            void init(namespace_cache_entry_t *_ref_target);
            void reset();
        private:
            namespace_cache_entry_t *ref_target;
        };
        namespace_cache_entry_t *cache_entry;
        ref_handler_t ref_handler;
        threadnum_t thread;
    };

protected:
    virtual namespace_cache_entry_t *get_cache_entry(const uuid_u &ns_id) = 0;

    struct namespace_cache_entry_t {
    public:
        promise_t<namespace_interface_t *> namespace_if;
        int ref_count;
        cond_t *pulse_when_ref_count_becomes_zero;
        cond_t *pulse_when_ref_count_becomes_nonzero;
    };
};

class namespace_repo_t : public base_namespace_repo_t, public home_thread_mixin_t {
private:
    struct namespace_cache_t;

public:
    namespace_repo_t(mailbox_manager_t *,
                     const boost::shared_ptr<semilattice_read_view_t<cow_ptr_t<namespaces_semilattice_metadata_t> > > &semilattice_view,
                     clone_ptr_t<watchable_t<change_tracking_map_t<peer_id_t, namespaces_directory_metadata_t> > >,
                     rdb_context_t *);
    ~namespace_repo_t();

private:
    void create_and_destroy_namespace_interface(
            namespace_cache_t *cache,
            const uuid_u &namespace_id,
            auto_drainer_t::lock_t keepalive)
            THROWS_NOTHING;
    void on_namespaces_change();

    base_namespace_repo_t::namespace_cache_entry_t *get_cache_entry(const uuid_u &ns_id);

    mailbox_manager_t *mailbox_manager;
    boost::shared_ptr<semilattice_read_view_t<cow_ptr_t<namespaces_semilattice_metadata_t> > > namespaces_view;
    clone_ptr_t<watchable_t<change_tracking_map_t<peer_id_t, namespaces_directory_metadata_t> > > namespaces_directory_metadata;
    rdb_context_t *ctx;
    semilattice_read_view_t<cow_ptr_t<namespaces_semilattice_metadata_t> >::subscription_t namespaces_subscription;

    one_per_thread_t<std::map<namespace_id_t, std::map<key_range_t, machine_id_t> > > region_to_primary_maps;
    one_per_thread_t<namespace_cache_t> namespace_caches;

    DISABLE_COPYING(namespace_repo_t);

    auto_drainer_t drainer;
};

#endif /* CLUSTERING_ADMINISTRATION_NAMESPACE_INTERFACE_REPOSITORY_HPP_ */
