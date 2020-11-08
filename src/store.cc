#include <stdio.h>         // printf
#include "options.h"       // options and default settings
#include "utils.h"
#include "store.h"         // Store
#include "mail_handling.h"
#include "types.h"         // MailboxMap, Passwd, SUCCESS/FAILED
#include <cassert>         // assert

#include <iostream>        // only for debuging

extern Store*        match_pattern_store;
extern Passwd*       current_context_passwd;
extern enum operation_mode_t operation_mode;
extern options_t options;
extern int expunged_mails;

//////////////////////////////////////////////////////////////////////////
//
// Store
//
//////////////////////////////////////////////////////////////////////////
void Store::clear() {
  name      = "";
  server   = "";
  prefix   = "";
  ref      = "";
  pat      = "";
  isremote = 0;
  delim    = '!';
  stream   = NIL;
  passwd.clear();
}

void Store::print(FILE* f) {
  fprintf( f, "store %s {", name.c_str() );
  if (server != "") {
    fprintf( f, "\n\tserver ");
    print_with_escapes( f, server);
  }
  if (prefix != "") {
    fprintf( f, "\n\tprefix ");
    print_with_escapes( f, prefix);
  }
  if (ref != "") {
    fprintf( f, "\n\tref ");
    print_with_escapes( f, ref);
  }
  if (pat != "") {
    fprintf( f, "\n\tpat ");
    print_with_escapes( f, pat);
  }
  if (! passwd.nopasswd) {
    fprintf( f, "\n\tpasswd ");
    print_with_escapes( f, passwd.text);
  }
  fprintf( f, "\n}\n");
}

//////////////////////////////////////////////////////////////////////////
//
size_t Store::acquire_mail_list()
//
// Acquire mailbox list for store corresponding to it's pattern
//
// boxes will be filled up through the callback function "mm_list"
// by c-client's mail_list function
//
// get_mail_list returns the number of mailboxes (that it found)
//
// This function corresponds to c-client's mail_list
//
//////////////////////////////////////////////////////////////////////////
{
  match_pattern_store = this;
  mail_list( this->stream, nccs(ref), nccs(pat));
  match_pattern_store = NULL;
 
  return boxes.size();
}

//////////////////////////////////////////////////////////////////////////
//
void Store::get_delim( )
//
// Determine the mailbox delimiter that "stream" is using.
// 
//////////////////////////////////////////////////////////////////////////
{
  Store store_tmp;
  store_tmp.stream = this->stream;
  store_tmp.server = this->server;
  store_tmp.ref    = this->ref;
  store_tmp.passwd = this->passwd;
  store_tmp.prefix = "";
  store_tmp.pat = "INBOX";
  store_tmp.acquire_mail_list();
  this->delim = store_tmp.delim;
}

//////////////////////////////////////////////////////////////////////////
//
string Store::full_mailbox_name(const string& box)
//
// Given the name of a mailbox, return its full IMAP name.
//
//////////////////////////////////////////////////////////////////////////
{
  string boxname = box;
  string fullbox;
  // Replace our DEFAULT_DELIMITER by the respective store delimiter
  for (unsigned int i=0; i<boxname.size(); i++)
    if( boxname[i] == DEFAULT_DELIMITER) boxname[i] = delim;
  fullbox = server + prefix + boxname;
  return fullbox;
}

//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* Store::store_open( long c_client_options)
//
// Opens the store with "c_client_options" options.
//
// The function will complain to STDERR on error.
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////
{
  current_context_passwd = &passwd;

  if (options.debug)
    printf("Opening %s with options %ld\n",
            this->server.c_str(), 
            c_client_options | ( options.debug_imap ? OP_DEBUG : 0 ));
  stream = mail_open( this->stream, nccs(this->server),
                      c_client_options | ( options.debug_imap ? OP_DEBUG : 0 ));
  if (! this->stream) {
    fprintf( stderr, "Error: Can't contact server %s\n", this->server.c_str());
  }
  return this->stream;
}

//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* Store::mailbox_open( const string& boxname,
                                 long c_client_options)
//
// Opens the mailbox "boxname" inside with "c_client_options" options.
//
// The function will complain to STDERR on error.
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////
{
  string fullboxname = this->full_mailbox_name(boxname);
  current_context_passwd = &passwd;
  
  if (options.debug)
    printf("Opening %s with options %ld\n",
            fullboxname.c_str(), 
            c_client_options);
  stream = ::mailbox_open( this->stream, fullboxname, c_client_options);
  if (! this->stream) {
    fprintf( stderr, "Error: Couldn't open %s\n", fullboxname.c_str());
  }
  
  return this->stream;
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::mailbox_create( const string& boxname )
//
// Creates the mailbox "boxname" in the store
//
// Returns false on failure.
//
//////////////////////////////////////////////////////////////////////////
{
  string fullboxname = this->full_mailbox_name(boxname);
  current_context_passwd = &passwd;
  
  if ( options.simulate ) { // just fail if simulating
    printf( "Creating %s in %s\n", boxname.c_str(), this->name.c_str());
    return false;
  }
  
  bool res;
  bool error_tmp = options.log_error;
  options.log_error = false;
  res = mail_create( this->stream, nccs( fullboxname.c_str() ) );
  options.log_error = error_tmp;
  
  if ( options.debug) {
    if ( res )
      printf( "Created %s in %s\n", boxname.c_str(), this->name.c_str());
    else
      printf( "Failed to create %s in %s\n", boxname.c_str(), name.c_str());
  }

  return res;
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::fetch_message_ids(MsgIdPositions& mids, MsgIdSet& remove_set)
//
// Fetch all the message ids that the currently open mailbox contains.
// 
// If there are duplicates they will be added to the remove_set
// (depending on the compile time option expunge_duplicates)
//
// returns:
//              0              - failure
//              1              - success
//              mids           - a hash indexed by msgid containing the
//                               position of the message in the mailbox
//
//////////////////////////////////////////////////////////////////////////
{
  // loop and fetch all the message ids from a mailbox
  unsigned long n = this->stream->nmsgs;
  unsigned long nabsent = 0, nduplicates = 0;

  if (options.debug) {
    printf( " Fetching message id's in mailbox \"%s\"\n", 
            this->stream->mailbox);
  }

  for (unsigned long msgno=1; msgno<=n; msgno++) {
    MsgId msgid;
    ENVELOPE *envelope;
    bool isdup;

    envelope = mail_fetchenvelope( this->stream, msgno);
    if (! envelope) {
      fprintf( stderr,
               "Error: Couldn't fetch envelope #%lu from mailbox box %s\n",
               msgno, this->stream->mailbox);
      fprintf( stderr, "       Aborting!\n");
      return 0;
    }
    msgid = MsgId(envelope);
    if (msgid.length() == 0) {
      print_lead("no msg-id", "");
      nabsent++;
      // Absent message-id.  Don't touch message.
      continue;
    }
    isdup = mids.count( msgid );
    if (isdup) {
      if ( options.expunge_duplicates ) {
        char seq[30];
        sprintf( seq, "%lu", msgno);
        if (! options.simulate )
          if ( msgid.empty() )
            fprintf( stderr, "Not deleting duplicate message with empty "
                             "Message-ID - see README");
          else
            remove_set.insert( msgid );
            // mail_setflag( this->stream, seq, "\\Deleted" );
      }
      nduplicates++;
      if ( options.show_from ) print_lead( "duplicate", "");
    }
    else
    {
      mids.insert(make_pair(msgid, msgno));
    }
    if ( isdup && options.show_from )
    {
      print_from( this->stream, msgno );
      if ( options.show_message_id ) print_msgid( msgid.c_str() );
      printf("\n");
    }
  }

  if (nduplicates)
  {
    if (options.show_summary)
    {
      printf( "%lu duplicate%s", nduplicates, nduplicates==1 ? "" : "s" );
      if ( operation_mode != mode_diff ) {
        printf(" in %s", name.c_str());
      }
      printf(", ");
    }
    else
    {
      printf( "%lu duplicate%s for deletion in %s\n",
              nduplicates,
              nduplicates==1 ? "" : "s",
              name.c_str() );
    }
    fflush(stdout);
  }
  return 1;
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::list_contents()
//
// Display contents of currently open mailbox
// 
// The mailbox is defined by the name in "box" and by the "store" where the
// mailbox resides
//
// returns: FAILED/SUCCESS
//
// This was copied from fetch_message_ids
//
//////////////////////////////////////////////////////////////////////////
{
  MsgIdPositions mids;

  // loop and fetch all the message ids from a mailbox
  unsigned long n = this->stream->nmsgs;
  for (unsigned long msgno=1; msgno<=n; msgno++) {
    MsgId msgid;
    ENVELOPE *envelope;
    bool isdup;

    envelope = mail_fetchenvelope( this->stream, msgno);

    if (! envelope) {
      fprintf( stderr,
               "Error: Couldn't fetch envelope #%lu from mailbox box %s\n",
               msgno, this->stream->mailbox);
      fprintf( stderr,
               "       Aborting!\n");
      return FAILED;
    }
    msgid = MsgId(envelope);
    if (msgid.length() == 0)
      print_lead( "no msg-id", "");
    else
    {
      isdup = mids.count( msgid );
      if (isdup)
        print_lead( "duplicate", "");
      else
        mids.insert( make_pair(msgid, msgno));
    
      if ( options.show_message_id ) print_msgid( msgid.c_str() );
    }
    print_from( this->stream, msgno );
    printf( "\n");
  }
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::flag_message_for_removal( unsigned long msgno,
                                     const MsgId& msgid,
                                     char const * place)
//
// returns !0 on success
//
//////////////////////////////////////////////////////////////////////////
{
  MsgId msgid_fetched;
  ENVELOPE *envelope;
  bool success = 1;
  
  current_context_passwd = &passwd;
  envelope = mail_fetchenvelope( this->stream, msgno );
  if (! envelope) {
    fprintf( stderr,
             "Error: Couldn't fetch envelope #%lu from mailbox box %s\n",
             msgno, this->stream->mailbox);
    return 0;
  }
  msgid_fetched = MsgId(envelope);
  if (msgid_fetched.length() == 0) {
    printf( "Error: no message-id, so I won't delete the message.\n" );
    // Possibly indicates concurrent access?
    success = 0;
  }
  else
  {
    if( msgid_fetched != msgid )
    {
      printf( "Error: message-ids %s and %s don't match, "
              "so I won't delete the message.\n",
              msgid_fetched.c_str(), msgid.c_str() );
      success = 0;
    }
  }
  
  if (success)
  {
    char seq[30];
    sprintf( seq, "%lu", msgno);
    if (! options.simulate ) mail_setflag( this->stream, seq, "\\Deleted" );
  }
  if (options.show_from)
  {
    if (success) {
      print_lead( "deleted", place);
    }
    else {
      print_lead( "deletefail", "");
    }
    print_from( this->stream, msgno );
    printf("\n");
  }
  return success;
}

//////////////////////////////////////////////////////////////////////////
//
char* Store::driver_name()
//
// Return the name of the driver we're using for accessing a store.
//
// The store must contain at least one valid mailbox name, otherwise the call
// will return NULL.
//
// Returns NULL if no valid driver was found
//
//////////////////////////////////////////////////////////////////////////
{
  DRIVER* drv;
  char* full_name; 

  if ( boxes.size() == 0 ) // boxes list is empty (no box with that name
                           // exists)
    drv = mail_valid( this->stream, "", NIL);
  else
    drv = mail_valid( this->stream,
                      nccs( full_mailbox_name( boxes.begin()->first) ),
                      NIL);
  if (drv)
    return drv->name;
  else
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void Store::display_driver()
//
// Display which drivers we're using for accessing a store
//
// The store must contain at least one valid mailbox name, otherwise the call
// will return NULL.
//
//////////////////////////////////////////////////////////////////////////
{
  char* the_driver_name = driver_name();

  if (the_driver_name)
    printf( "Using driver %s for store %s\n", the_driver_name, nccs( name));
  else
    printf( "No driver for store %s found\n", nccs( name) );
}

//////////////////////////////////////////////////////////////////////////
//
void Store::print_error(const char * cause, const string& mailbox)
//
//////////////////////////////////////////////////////////////////////////
{
  fprintf( stderr,
           "Error: Couldn't access mailbox \"%s\" in store \"%s\" for %s\n",
           mailbox.c_str(), name.c_str(), cause );       
  fprintf( stderr,
           "       Continuing with next mailbox\n");
}

//////////////////////////////////////////////////////////////////////////
//
int Store::mailbox_expunge( string mailbox_name )
//
//////////////////////////////////////////////////////////////////////////
{
  expunged_mails = 0; // is manipulated by the c-client callback mm_expunge
  mail_expunge( this->stream );
  return expunged_mails;
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::open_read_only_connection( )
//
// returns FAILED/SUCCESS
//
//////////////////////////////////////////////////////////////////////////
{
  if ( this->isremote )
  {
    if (! this->store_open( OP_HALFOPEN | OP_READONLY) )
    {
      fprintf( stderr, "Error: Could not open a half open, read only connection to store %s\n",
               this->name.c_str() );
      return FAILED;
    }
  }
  else
  {
    this->stream = NULL;
  }
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
void Store::acquire_mailboxes_and_delimiter( /*in*/ const bool debug )
//
//////////////////////////////////////////////////////////////////////////
{
  // Display which drivers we're using for accessing the store
  if (debug) {
    this->display_driver();
  }

  if (debug) printf( " Items in store \"%s\":\n", this->name.c_str() );

  if (! this->acquire_mail_list() && options.log_warn) {
    // TODO: fail here?
    printf( " Store pattern doesn't match any selectable mailbox\n");
  }

  if (this->delim == '!') {
    this->get_delim();
  }

  if (this->delim == '!') { // this should not happen
    assert(0);
  }
  else if (debug) {
    // delim can be '' for INBOXes
    if ( ! this->delim )
      printf(" No delimiter found for store \"%s\"\n", this->name.c_str());
    else
      printf( " Delimiter for store \"%s\" is '%c'\n",
              this->name.c_str(), this->delim );
  }
}

//////////////////////////////////////////////////////////////////////////
//
bool Store::list_mails( const options_t* options )
//
// returns: FAILED/SUCCESS
//
//////////////////////////////////////////////////////////////////////////
{
  bool return_status = SUCCESS;

  if ( this->open_read_only_connection() == FAILED ) {
    return_status = FAILED;
  }
  else {
    this->acquire_mailboxes_and_delimiter( options->debug );

    if ( options->show_from | options->show_message_id ) {
      for ( MailboxMap::iterator curr_mbox = this->boxes.begin() ; 
            curr_mbox != this->boxes.end() ;
            curr_mbox++ )
      {
        printf("\nMailbox: %s\n", curr_mbox->first.c_str());
        if( curr_mbox->second.no_select )
          printf("  not selectable\n");
        else {
          // TODO: shouldn't this be OP_READONLY
          this->stream = this->mailbox_open( curr_mbox->first, 0);
          if (! this->stream) break;
          if (this->list_contents() == FAILED)
            return_status = FAILED;
        }
      }
    }
    else {
      print_list_with_delimiter(this->boxes, stdout, "\n");
    } 
  }
  return return_status;
} 
