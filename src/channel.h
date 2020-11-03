#ifndef __MAILSYNC_CHANNEL__

#include <stdio.h>
#include <string>
#include "types.h"      // Passwd
#include "store.h"

enum direction_t { a_to_b, b_to_a };

//////////////////////////////////////////////////////////////////////////
//
class Channel
//
// Structure that holds two Stores (sets of mailboxes) that should be synched
//
//////////////////////////////////////////////////////////////////////////
{
  public: // constructor
    Channel(): name(), msinfo(), passwd(), sizelimit(0) {};

  public: // methods
    void print(FILE* f);

    void set_passwd    (      string  password) {  passwd.set_passwd(password);                }
    void set_sizelimit (const string& sizelim)  {  sizelimit=strtoul(sizelim.c_str(),NULL,10); }

    bool read_seen_last_time  (       MsgIdsPerMailbox&  mids_per_box, 
                                      MailboxMap&        deleted_mailboxes);
    bool write_seen_this_time ( const MailboxMap&        deleted_mailboxes,
                                      MsgIdsPerMailbox&  thistime);

    bool open_for_copying   ( string             mailbox_name,
		              enum direction_t   direction);
    bool copy_message       ( unsigned long      msgno,
                              const MsgId&       msgid,
                              string             mailbox_name,
                              enum direction_t   direction);
    void list_mailboxes();

  private:
    bool has_channel_format ( const ENVELOPE* envelope);

  public: // attributes
    string         name;
    Store          store_a;
    Store          store_b;
    string         msinfo;
    Passwd         passwd;
    unsigned long  sizelimit;
};

#define __MAILSYNC_CHANNEL__
#endif
