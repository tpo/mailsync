#ifndef __MAILSYNC_LIST_MAILS_IN_STORE__

#include <stdio.h>             // printf

#include "options.h"           // options and default settings
#include "types.h"             // MailboxMap, Passwd
#include "store.h"             // Store
#include "mail_handling.h"     // print_list_with_delimiter



//////////////////////////////////////////////////////////////////////////
//
// returns:
// * 0 on failure
// * 1 on success
//
bool list_mails_in_store(options_t* options, Store& store_a) {
    if ( options->show_from | options->show_message_id ) {
      for ( MailboxMap::iterator curr_mbox = store_a.boxes.begin() ; 
            curr_mbox != store_a.boxes.end() ;
            curr_mbox++ )
      {
        printf("\nMailbox: %s\n", curr_mbox->first.c_str());
        if( curr_mbox->second.no_select )
          printf("  not selectable\n");
        else {
          // TODO: shouldn't this be OP_READONLY
          store_a.stream = store_a.mailbox_open( curr_mbox->first, 0);
          if (! store_a.stream) break;
          if (! store_a.list_contents() )
            return 0;
        }
      }
    }
    else {
      print_list_with_delimiter(store_a.boxes, stdout, "\n");
    } 
    return 1;
} 

#endif
