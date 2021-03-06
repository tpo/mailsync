/// Please use spaces instead of tabs

// See the documentation: "HACKING" and "ABSTRACT"

#include "config.h"             // include autoconf settings

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
extern int errno;               // Just in case

#include <string>
#include <set>
#include <map>
#include <vector>
#include <cassert>
using std::string;
using std::set;
using std::map;
using std::vector;
using std::make_pair;

#include "c-client-header.h"

#include "configuration.h"       // configuration parsing and setup
#include "options.h"             // options and default settings
#include "commandline.h"         // commandline parsing
#include "types.h"               // MailboxMap
#include "password.h"            // Password
#include "store.h"               // Store
#include "channel.h"             // Channel
#include "mail_handling.h"       // functions implementing various
                                 // synchronization steps and helper functions

//------------------------------- Defines  -------------------------------

#define CREATE   1
#define NOCREATE 0

//------------------------ Global Variables ------------------------------

// current operation mode
enum operation_mode_t operation_mode = mode_unknown;
// options and default settings 
//
// These are global, because we need to have access to them
// in callback functions (f.ex. mm_list) for c-client, where
// the callback interface does not allow to pass the options
// into the callback.
options_t options;

// won't link correctly if this is static - why?
Store*       match_pattern_store;

//////////////////////////////////////////////////////////////////////////
// The password for the current context
// Required, because we don't know inside the c-client callback functions
// which context (store1, store2, channel) we are in
Password * current_context_passwd = NULL;
//////////////////////////////////////////////////////////////////////////

bool parse_arguments_read_config_file_choose_operation_mode( /*in*/  const int    argc,
                                                             /*in*/  const char** argv,
                                                             /*out*/ Channel*     channel)
{

  string config_file;
  vector<string> channels_and_stores;
  // bad command line parameters
  if (! read_commandline_options( argc,
                                  argv,
                                  options,
                                  channels_and_stores,
                                  config_file) )
  {
    return FAILED;
  }

  operation_mode = setup_channel_stores_and_mode( config_file,
                                                  channels_and_stores,
                                                  *channel );
  if ( operation_mode == mode_unknown )
  {
    return FAILED;
  }

  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
int main(const int argc, const char** argv)
//
//////////////////////////////////////////////////////////////////////////
{
  Channel channel;
  Store& store_a = channel.store_a;
  Store& store_b = channel.store_b;

#include "linkage.c"

  if( parse_arguments_read_config_file_choose_operation_mode( /*in const*/ argc,
                                                              /*in const*/ argv,
                                                              /*out*/      &channel )
      == FAILED )
  {
    exit(1);
  }

  store_a.boxes.clear();
  store_b.boxes.clear();

  // initialize c-client environment (~/.imaprc etc.)
  env_init( getenv("USER"), getenv("HOME"));

  // TODO: unify NIL / NULL 

  if ( operation_mode == mode_list ) {

    store_a.list( options.debug,
                  options.show_from, 
                  options.show_message_id ) == SUCCESS ?
      exit(0) : exit(1);

  }
  else // operation_mode == mode_sync || mode_diff
  {
    MsgIdsPerMailbox lasttime, thistime;
    MailboxMap deleted_mailboxes;   // present last time, but not this time
    MailboxMap empty_mailboxes;
    int success;
    bool& debug = options.debug;

    if ( store_a.open_read_only_connection() == FAILED )
      exit(1);

    store_a.acquire_mailboxes_and_delimiter( debug );

    if ( store_b.open_read_only_connection() == FAILED )
      exit(1);

    store_b.acquire_mailboxes_and_delimiter( debug );

    if (debug)
      channel.list_mailboxes(); // Display all the mailboxes we've found

    // Read in what mailboxes and messages we've seen the last time
    // we've synchronized
    if (! channel.read_seen_last_time( lasttime, deleted_mailboxes) )
      exit(1);    // failed to read in msinfo or similar


    // Iterate over all mailboxes and sync or diff each
    //
    // our comparison operator for our stores compares lenghts
    // that means that we're traversing the store from longest to
    // shortest mailbox name - this makes sure that we'll first see
    // and create mailboxes with longer "path"names that means 
    // submailboxes first
    success = 1; // TODO: this is bogus isn't it?
    for ( MailboxMap::iterator curr_mbox = store_a.boxes.begin(); 
          curr_mbox != store_b.boxes.end();
          curr_mbox++ )
    {
      if ( curr_mbox == store_a.boxes.end()) { // if we're done with store_a
        curr_mbox = store_b.boxes.begin();     // continue with store_b
        if ( curr_mbox == store_b.boxes.end()) break;
      }

      // skip if the current mailbox has allready been synched
      if ( mailbox_properties(curr_mbox).done)
        continue;
      
      // if mailbox doesn't exist in either one of the stores -> create it
      if ( store_a.boxes.find( mailbox_name(curr_mbox) ) == store_a.boxes.end() )
        if ( ! store_a.mailbox_create( mailbox_name(curr_mbox) ) )
          continue;
      if ( store_b.boxes.find( mailbox_name(curr_mbox) ) == store_b.boxes.end() )
        if ( ! store_b.mailbox_create( mailbox_name(curr_mbox) ) )
          continue;

      // when traversing store_a's boxes we don't need to worry about
      // whether it has been synched yet or not.  It isn't unless we're
      // in store_b that it matters whether the current mailbox has been
      // traversed in store_a allready
      mailbox_properties(store_b.boxes.find(mailbox_name(curr_mbox))).done = true;

      // skip unselectable (== can't contain mails) boxes
      if ( mailbox_properties(store_a.boxes.find( mailbox_name(curr_mbox) )).no_select ) {
        if ( debug )
          printf( "%s is not selectable: skipping\n", mailbox_name(curr_mbox).c_str() );
        continue;
      }
      if ( mailbox_properties(store_b.boxes.find( mailbox_name(curr_mbox) )).no_select ) {
        if ( debug )
          printf( "%s is not selectable: skipping\n", mailbox_name(curr_mbox).c_str() );
        continue;
      }

      if (options.show_from)
        printf("\n *** %s ***\n", mailbox_name(curr_mbox).c_str());

      MsgIdSet msgids_lasttime( lasttime[mailbox_name(curr_mbox)] ), msgids_union, msgids_now;
      MsgIdPositions msgidpos_a, msgidpos_b;

      if (options.show_summary) {
        printf("%s: ", mailbox_name(curr_mbox).c_str());
        fflush(stdout);
      }
      else {
        printf("\n");
      }

      // fetch_message_ids(): map message-ids to message numbers
      //                      and optionally remember duplicates. 
      //
      // Attention: from here on we're operating on streams to single
      //            _mailboxes_! That means that from here on
      //            streamx_stream is connected to _one_ specific
      //            mailbox.
     
      // Messges that should be removed in store_a respectively in store_b
      MsgIdSet remove_a, remove_b;

      // open and fetch message ID's from the mailbox in the first store
      store_a.stream = store_a.mailbox_open( mailbox_name(curr_mbox), OP_READONLY );
      if (! store_a.stream)
      {
        store_a.print_error( "opening and writing", mailbox_name(curr_mbox));
        continue;
      }
      if (! store_a.fetch_message_ids( msgidpos_a , remove_a) )
      {
        store_a.print_error( "fetching of mail ids", mailbox_name(curr_mbox));
        continue;
      }

      // if we're in sync mode open and fetch message IDs from the
      // mailbox in the second store
      if( operation_mode == mode_sync ) {
        store_b.stream = store_b.mailbox_open( mailbox_name(curr_mbox), OP_READONLY);
        if (! store_b.stream) {
          store_b.print_error( "fetching of mail ids", mailbox_name(curr_mbox));
          continue;
        }
        if (! store_b.fetch_message_ids( msgidpos_b, remove_b )) {
          store_b.print_error( "fetching of mail ids", mailbox_name(curr_mbox));
          continue;
        }
      } else if( operation_mode == mode_diff ) {
        for( MsgIdSet::iterator i=msgids_lasttime.begin();
             i!=msgids_lasttime.end();
             i++ )
        {
             msgidpos_b[*i] = 0;
        }
      }

      // Create the set of all seen message IDs in a mailbox:
      // + message IDs seen the last time
      // + message IDs seen in the mailbox from store_a
      // + message IDs seen in the mailbox from store_b
      // 
      // msgids_union = union(msgids_lasttime, msgids_a, msgids_b)
      msgids_union = msgids_lasttime;
      for( MsgIdPositions::iterator i = msgidpos_a.begin();
           i != msgidpos_a.end() ;
           i++ )
      {
        msgids_union.insert(i->first);
      }
      for( MsgIdPositions::iterator i = msgidpos_b.begin();
           i != msgidpos_b.end();
           i++)
      {
        msgids_union.insert(i->first);
      }

      // Messages that should be copied from store_a to store_b,
      // from store_b to store_a
      MsgIdSet copy_a_b, copy_b_a;

      // Iterate over all messages that were seen in a mailbox last time,
      // in store_a and in store_b
      for ( MsgIdSet::iterator i=msgids_union.begin();
            i!=msgids_union.end();
            i++ )
      {
        // determine first what to do with a message
        bool in_a = msgidpos_a.count(*i);
        bool in_b = msgidpos_b.count(*i);
        bool in_l = msgids_lasttime.count(*i);

        int a_b_l = (  (in_a ? 0x100 : 0) 
                     + (in_b ? 0x010 : 0)
                     + (in_l ? 0x001 : 0) );

        switch (a_b_l) {

        case 0x100:  // New message on a
          copy_a_b.insert(*i);
          msgids_now.insert(*i);
          break;

        case 0x010:  // New message on b
          copy_b_a.insert(*i);
          msgids_now.insert(*i);
          break;

        case 0x111:  // Kept message
        case 0x110:  // New message, present in a and b, no copying
                     // necessary
          msgids_now.insert(*i);
          break;

        case 0x101:  // Deleted on b
          remove_a.insert(*i);
          break;

        case 0x011:  // Deleted on a
          remove_b.insert(*i);
          break;

        case 0x001:  // Deleted on both
          break;

        case 0x000:  // Shouldn't happen
        default:
          assert(0);
          break;
        }


      }

      unsigned long now_n = msgids_now.size();

      switch (operation_mode) {
      
       /////////////////////////// mode_sync ///////////////////////////
      
       case mode_sync:
        {
          bool success;
          unsigned long removed_a = 0, removed_b = 0, copied_a_b = 0,
                        copied_b_a = 0;

          //////////////////// copying messages ///////////////////////
          
          if (debug)
            printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                    store_a.name.c_str(), store_b.name.c_str() );

          if (! channel.open_for_copying( mailbox_name(curr_mbox), a_to_b) )
            exit(1);
          for ( MsgIdSet::iterator i =copy_a_b.begin(); i !=copy_a_b.end(); i++) {
            success = channel.copy_message( msgidpos_a[*i], *i,
                                            mailbox_name(curr_mbox), a_to_b );
            if (success) copied_a_b++;
            else         msgids_now.erase(*i);
            // if we've failed to copy the message over we'll pretend that we
            // haven't seen it at all. That way mailsync will have to rediscover
            // and resync the same message again next time
          }

          if (debug)
            printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                    store_b.name.c_str(), store_a.name.c_str() );

          if (! channel.open_for_copying( mailbox_name(curr_mbox), b_to_a) )
            exit(1);
          for ( MsgIdSet::iterator i=copy_b_a.begin(); i !=copy_b_a.end(); i++) {
            success = channel.copy_message( msgidpos_b[*i], *i,
                                            mailbox_name(curr_mbox), b_to_a );
            if (success) copied_b_a++;
            else         msgids_now.erase(*i);
          }
          
          printf("\n");
          if (copied_a_b) printf( "%lu copied %s->%s.\n", copied_a_b,
                                  store_a.name.c_str(), store_b.name.c_str() );
          if (copied_b_a) printf( "%lu copied %s->%s.\n", copied_b_a,
                                  store_b.name.c_str(), store_a.name.c_str() );
          if (removed_a)  printf( "%lu deleted on %s.\n",
                                  removed_a, store_a.name.c_str() );
          if (removed_b)  printf( "%lu deleted on %s.\n",
                                  removed_b, store_b.name.c_str() );
          if (options.show_summary) {
            printf( "%lu remain%s.\n", now_n, now_n != 1 ? "" : "s");
            fflush(stdout);
          } else {
            printf( "%lu messages remain in %s\n",
                    now_n, mailbox_name(curr_mbox).c_str() );
          }

          //////////////////// removing messages ///////////////////////

          if ( options.delete_messages && (! options.simulate) ) {
          
            if (debug) printf( " Removing messages from store \"%s\"\n",
                               store_a.name.c_str() );

            // TODO: check first if there are any messages to be removed before
            //       opening
            store_a.stream = store_a.mailbox_open( mailbox_name(curr_mbox), 0 );
            if (! store_a.stream)
            {
              store_a.print_error( "opening for removal ", mailbox_name(curr_mbox));
            }
            else
              for( MsgIdSet::iterator i =remove_a.begin(); i !=remove_a.end(); i++) {
                success = store_a.flag_message_for_removal( msgidpos_a[*i], *i, "< ");
                if (success) removed_a++;
              }
        
            if (debug) printf( " Removing messages from store \"%s\"\n",
                               store_b.name.c_str() );

            // TODO: check first if there are any messages to be removed before
            //       opening
            store_b.stream = store_b.mailbox_open( mailbox_name(curr_mbox), 0 );
            if (! store_b.stream)
            {
              store_a.print_error( "opening for removal ", mailbox_name(curr_mbox));
            }
            else
              for( MsgIdSet::iterator i =remove_b.begin(); i !=remove_b.end(); i++) {
                success = store_b.flag_message_for_removal( msgidpos_b[*i], *i, "> ");
                if (success) removed_b++;
              }

            //////////////////////// expunging emails /////////////////////////
            // this *needs* to be done *after* coying as the *last* step
            // otherwise the order of the mails will get messed up since
            // some random messages inbewteen have been deleted in the mean
            // time and the message numbers we know don't correspond to
            // messages in the mailbox/store any more
          
            if (debug) printf( " Expunging messages\n" );

            int n_expunged_a = store_a.mailbox_expunge( mailbox_name(curr_mbox) );
            int n_expunged_b = store_b.mailbox_expunge( mailbox_name(curr_mbox) );
            if (n_expunged_a) printf( "Expunged %d mail%s in store %s\n"
                                    , n_expunged_a
                                    , n_expunged_a == 1 ? "" : "s"
                                    , store_a.name.c_str() );
            if (n_expunged_b) printf( "Expunged %d mail%s in store %s\n"
                                    , n_expunged_b
                                    , n_expunged_b == 1 ? "" : "s"
                                    , store_b.name.c_str() );
          }

          //////////////////////// deleting empty mailboxes /////////////////////////
          
          if (options.delete_empty_mailboxes) {
            if (now_n == 0) {
              // add empty mailbox to empty_mailboxes
              empty_mailboxes[ mailbox_name(curr_mbox) ];
              deleted_mailboxes[ mailbox_name(curr_mbox) ];
            }
          }
        } // end case mode_sync
        break;

       /////////////////////////// mode_diff ///////////////////////////
      
       case mode_diff:
        {
          if ( copy_a_b.size() )
            printf( "%d new, ", copy_a_b.size() );
          if (remove_b.size())
            printf( "%d deleted, ", remove_b.size() );
          printf( "%d currently at store %s.\n",
                  msgids_now.size(), store_b.name.c_str());
        }
        break;

       default:
        break;
      }

      thistime[mailbox_name(curr_mbox)] = msgids_now;

//   TODO: why do we want to close the boxes?
//   instead of expunging emails we could also use mail_open(OP_EXPUNGE) instead...
//      // close local boxes
//      if (!store_a.isremote)
//        store_a.stream = mail_close(store_a.stream);
//      if (store_b.stream && !store_b.isremote)
//        store_b.stream = mail_close(store_b.stream);

    } // end loop over all mailboxes

    if (store_a.isremote) store_a.stream = mail_close(store_a.stream);
    if (store_b.isremote) store_b.stream = mail_close(store_b.stream);

    // TODO: which success are we talking about? Above there are two instances
    //       of "success" declared which mask each other out...
    if (!success)
      return 1;

    if ( options.delete_empty_mailboxes && operation_mode==mode_sync )
    {
      string fullboxname;

      if (store_a.isremote) {
        store_a.stream = NIL;
        store_a.store_open( OP_HALFOPEN );
      } else {
        store_a.stream = NULL;
      }
      if (store_b.isremote) {
        store_b.stream = NIL;
        store_b.store_open( OP_HALFOPEN );
      } else {
        store_b.stream = NULL;
      }
      for ( MailboxMap::const_iterator mailbox = empty_mailboxes.begin() ;
            mailbox != empty_mailboxes.end() ;
            mailbox++ )
      {
        fullboxname = store_a.full_mailbox_name( mailbox_name(mailbox));
        printf("%s: deleting\n", mailbox_name(mailbox).c_str());
        printf("  %s", fullboxname.c_str());
        fflush(stdout);
        current_context_passwd = &(store_a.passwd);
        if (mail_delete(store_a.stream, nccs(fullboxname)))
          printf("\n");
        else
          printf(" failed\n");
        fullboxname = store_b.full_mailbox_name( mailbox_name(mailbox));
        printf("  %s", fullboxname.c_str());
        fflush(stdout);
        current_context_passwd = &(store_b.passwd);
        if (mail_delete(store_b.stream, nccs(fullboxname))) 
          printf("\n");
        else
          printf(" failed\n");
      }
    }

    if (operation_mode==mode_sync)
      if (!options.simulate)
        channel.write_seen_this_time( deleted_mailboxes, thistime);

    return 0;
  }
}
