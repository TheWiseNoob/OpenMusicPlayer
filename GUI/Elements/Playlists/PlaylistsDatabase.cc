/* ////////////////////////////////////////////////////////////////////////////   
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  The developer(s) of the OMP audio player hereby grant(s) permission
//  for non-GPL compatible GStreamer plugins to be used and distributed
//  together with GStreamer and OMP. This permission is above and beyond
//  the permissions granted by the GPL license by which OMP is covered.
//  If you modify this code, you may extend this exception to your version
//  of the code, but you are not obligated to do so. If you do not wish to do
//  so, delete this exception statement from your version.
//
//  Libraries used by OMP:
//
//    - clastfm: http://liblastfm.sourceforge.net/ 
//
//    - gstreamer: https://gstreamer.freedesktop.org/ 
//
//    - gtkmm: https://www.gtkmm.org/en/
//
//    - libconfig: http://www.hyperrealm.com/libconfig/
//
//    - standard C and C++ libraries
//
//    - taglib: http://taglib.org/
//
//////////////////////////////////////////////////////////////////////////// */





//         //
//         //
//         //
// Headers ////////////////////////////////////////////////////////////////////
//         //
//         //
//         //

//              //
//              //
// Class Header ///////////////////////////////////////////////////////////////
//              //
//              //

#include "PlaylistsDatabase.h"





//                 //
//                 //
// Program Headers ////////////////////////////////////////////////////////////
//                 //
//                 //

#include "../../../Base.h"

#include "../../../Metadata/Track.h"

#include "PlaylistColumnRecord.h"

#include "Playlists.h"

#include "PlaylistTreeStore.h"





//                 //
//                 //
// Outside Headers ////////////////////////////////////////////////////////////
//                 //
//                 //

#include <glibmm/main.h>

#include <gtkmm/progressbar.h>

#include <iostream>

#include <memory>

#include <pwd.h>

#include <sqlite3.h>

#include <sys/stat.h>

#include <sys/types.h>

#include <thread>

#include <unistd.h>





//        //
//        //
//        //
// Macros /////////////////////////////////////////////////////////////////////
//        //
//        //
//        //

namespace sigc
{ 

  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE

}





//            //
//            //
//            //
// Namespaces /////////////////////////////////////////////////////////////////
//            //
//            //
//            //

using namespace std;





//                 //
//                 //
//                 //
// Class Functions ////////////////////////////////////////////////////////////
//                 //
//                 //
//                 //

//             //
//             //
// Constructor ////////////////////////////////////////////////////////////////
//             //
//             //

PlaylistsDatabase::PlaylistsDatabase(Base& base_ref)

// Inherited Class

: Parts(base_ref)



// General

, database_ustr_ptr_(new Glib::ustring)

, quit_rebuilding_(false)

  {

  // 
  int result_code;



  // 
  struct passwd *pw = getpwuid(getuid());

  // 
  const char *homedir = pw -> pw_dir;

  // 
  std::string directory_str = homedir;

  // 
  directory_str += "/.omp";



  // 
  struct stat st; 

  // 
  if(!(stat(directory_str . c_str(),&st) == 0))
   {

    // 
    string mkdir_str = "mkdir " + directory_str;

    // 
    system(mkdir_str . c_str());

  }



  // 
  string db_str = directory_str + "/playlists.db";

  // 
  (*database_ustr_ptr_) = directory_str;



  // 
  result_code = sqlite3_open(db_str . c_str(), &database_);

  // 
  if(result_code)
  {

    stringstream debug_ss;

    debug_ss << "Can't open database: " << sqlite3_errmsg(database_);

    debug(debug_ss . str() . c_str());

  } 

  // 
  else
  {

    debug("Opened database successfully!");

  }



  // 
  Create_Playlist("Queue");

  // 
  Create_Playlist("Library");

}





//            //
//            //
// Destructor /////////////////////////////////////////////////////////////////
//            //
//            //

PlaylistsDatabase::~PlaylistsDatabase()
{

  // 
  sqlite3_close(database_);



  // 
  delete database_ustr_ptr_;

}





//                  //
//                  //
// Member Functions ////////////////////////////////////////////////////////////
//                  //
//                  //

bool PlaylistsDatabase::Add_Tracks
  (Glib::RefPtr<PlaylistTreeStore> playlist_treestore)
{

  // 
  string playlist_name_str = playlist_treestore -> get_name();

  // 
  const char* playlist_name = playlist_name_str . c_str();

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";

  // 
  int id = 0;



  // 
  Add_Column(playlist_name_str . c_str(), "DISC_NUMBER", "INT");

  // 
  Add_Column(playlist_name_str . c_str(), "DISC_TOTAL", "INT");



  // 
  for(auto it : playlist_treestore -> children())
  {

    // 
    (playlist_treestore -> appending_rows_done()) += 1;



    // 
    Gtk::TreeRow temp_treerow = it;

    // 
    shared_ptr<Track> track_sptr
      = temp_treerow[playlists() . playlist_column_record() . track_];



    // 
    temp_treerow[playlists() . playlist_column_record() . id_] = id;



    // 
    Glib::ustring* artists_str_ptr
      = Multiple_Values_Tag_Encode(track_sptr -> artists());

    // 
    Glib::ustring* album_artists_str_ptr
      = Multiple_Values_Tag_Encode(track_sptr -> album_artists());

    // 
    Glib::ustring* genres_str_ptr
      = Multiple_Values_Tag_Encode(track_sptr -> genres());



    // 
    sql += "INSERT INTO '";

    // 
    sql += Convert(playlist_name);

    // 
    sql += "' (ID, ALBUM, ALBUM_ARTIST, ARTIST, BIT_DEPTH, BIT_RATE, CHANNELS, CODEC, " \
           "DATE, DISC_NUMBER, DISC_TOTAL, DURATION, END, FILE_NAME, GENRE, " \
           "LENGTH_CS, MIME, PREGAP_START, REPLAY_GAIN_ALBUM_GAIN, " \
           "REPLAY_GAIN_ALBUM_PEAK, REPLAY_GAIN_TRACK_GAIN, " \
           "REPLAY_GAIN_TRACK_PEAK, SAMPLE_RATE, START, TITLE, " \
           "TRACK_NUMBER, TRACK_TOTAL, TYPE) " \
           "VALUES (" + to_string(id) + ", '"
             + Convert(track_sptr -> album()) + "', '"
             + Convert(album_artists_str_ptr -> raw()) + "', '"
             + Convert(artists_str_ptr -> raw()) + "', "
             + to_string(track_sptr -> bit_depth()) + ", "
             + to_string(track_sptr -> bit_rate()) + ", "
             + to_string(track_sptr -> channels()) + ", '"
             + Convert(track_sptr -> codec()) + "', "
             + to_string(track_sptr -> date()) + ", "
             + to_string(track_sptr -> disc_number()) + ", "
             + to_string(track_sptr -> disc_total()) + ", "
             + to_string(track_sptr -> duration()) + ", "
             + to_string(track_sptr -> end()) + ", '"
             + Convert(track_sptr -> filename()) + "', '"
             + Convert(genres_str_ptr -> raw()) + "', '"
             + Convert(track_sptr -> length()) + "', '"
             + Convert(track_sptr -> mime()) + "', "
             + to_string(track_sptr -> pregap_start()) + ", "
             + to_string(track_sptr -> replaygain_album_gain()) + ", "
             + to_string(track_sptr -> replaygain_album_peak()) + ", "
             + to_string(track_sptr -> replaygain_track_gain()) + ", "
             + to_string(track_sptr -> replaygain_track_peak()) + ", "
             + to_string(track_sptr -> sample_rate()) + ", "
             + to_string(track_sptr -> start()) + ", '"
             + Convert(track_sptr -> title()) + "', "
             + to_string(track_sptr -> track_number()) + ", "
             + to_string(track_sptr -> track_total()) + ", "
             + to_string(int(track_sptr -> type())) + ");";



    // 
    delete artists_str_ptr;

    // 
    delete genres_str_ptr;



    // 
    id++;

  }



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }



  // 
  return false;

}

bool PlaylistsDatabase::Add_Column
  (const char* playlist_name, const char* column_name, const char* type)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql;  /* Create SQL statement */



  // 
  sql = "ALTER TABLE ";

  // 
  sql += Convert(playlist_name);

  // 
  sql += " ADD COLUMN ";

  // 
  sql += column_name;

  // 
  sql += " ";

  // 
  sql += type;

  // 
  sql += ";";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}     

bool PlaylistsDatabase::Create_Playlist(const char* playlist_name)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql;  /* Create SQL statement */



  // 
  sql = "CREATE TABLE '";

  // 
  sql += Convert(playlist_name);

  // 
  sql += "'("  \
         "ID                       REAL PRIMARY KEY   NOT NULL," \
         "ALBUM                    TEXT," \
         "ALBUM_ARTIST             TEXT," \
         "ARTIST                   TEXT," \
         "BIT_DEPTH                INT               NOT NULL," \
         "BIT_RATE                 INT               NOT NULL," \
         "DATE                     INT               NOT NULL," \
         "DISC_NUMBER              INT               NOT NULL," \
         "DISC_TOTAL               INT               NOT NULL," \
         "DURATION                 INT               NOT NULL," \
         "CHANNELS                 INT               NOT NULL," \
         "CODEC                    TEXT              NOT NULL," \
         "END                      INT               NOT NULL," \
         "FILE_NAME                TEXT              NOT NULL," \
         "GENRE                    TEXT," \
         "LENGTH_CS                TEXT              NOT NULL," \
         "MIME                     TEXT              NOT NULL," \
         "PREGAP_START             INT               NOT NULL," \
         "REPLAY_GAIN_ALBUM_GAIN   REAL," \
         "REPLAY_GAIN_ALBUM_PEAK   REAL," \
         "REPLAY_GAIN_TRACK_GAIN   REAL," \
         "REPLAY_GAIN_TRACK_PEAK   REAL," \
         "SAMPLE_RATE              INT               NOT NULL," \
         "START                    INT               NOT NULL," \
         "TITLE                    TEXT," \
         "TRACK_NUMBER             INT," \
         "TRACK_TOTAL              INT," \
         "TYPE                     INT               NOT NULL);";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}     

bool PlaylistsDatabase::Cleanup_Database()
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";



  // 
  sql = "VACUUM;";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0, 0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}    

std::string PlaylistsDatabase::Convert(std::string raw_str)
{

  // 
  string new_str;

  // 
  for(auto it : raw_str)
  { 

    // 
    if(it == '\'')
    {

      // 
      new_str += "''";

    }

    // 
    else
    {

      // 
      new_str += it;

    } 

  }



  // 
  return new_str;

}

bool PlaylistsDatabase::Clear_Playlist(const char* playlist_name)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";



  // 
  sql = "DELETE FROM '";
  // 
  sql += Convert(playlist_name);

  sql += "';";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}    

bool PlaylistsDatabase::Delete_Playlist(const char* playlist_name)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";



  // 
  sql = "DROP TABLE '";

  // 
  sql += Convert(playlist_name);

  sql += "'; VACUUM;";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}    

bool PlaylistsDatabase::Delete_Rows
  (const char* playlist_name, std::vector<int> ids)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";



  for(auto it : ids)
  {

    // 
    sql += "DELETE from '";

    // 
    sql += Convert(playlist_name);

    // 
    sql += "' where ID=";

    // 
    sql += to_string(it) + ";";

  }

  


  // 
  result_code = sqlite3_exec(database_, sql . c_str(), 0,
                             0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}

bool PlaylistsDatabase::Extract_Tracks
  (Glib::RefPtr<PlaylistTreeStore> playlist_treestore)
{ 

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql;



  // 
  sql = "SELECT * from '";

  // 
  sql += Convert(playlist_treestore -> get_name());

  // 
  sql+= "';";



  // 
  result_code = sqlite3_exec(database_, sql . c_str(), Extract_Tracks_Callback,
                             &(playlist_treestore -> add_track_queue()),
                             &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}

int PlaylistsDatabase::Extract_Tracks_Callback
  (void* tracks_and_ids_vptr, int argc, char **argv, char **column_name)
{

  // 
  auto ids_and_tracks_ptr
    = (list<pair<int, shared_ptr<Track>>>*)(tracks_and_ids_vptr);



  // 
  shared_ptr<Track> new_track_sptr(new Track);

  // 
  int id = -1;



  // 
  for(int i = 0; i < argc; i++)
  {

    // 
    if(strcmp(column_name[i], "ID") == 0)
    {

      // 
      id = atoi(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "ALBUM") == 0)
    {

      // 
      new_track_sptr -> set_album(argv[i]);

    } 

    // 
    else if(strcmp(column_name[i], "ALBUM_ARTIST") == 0)
    {

      // 
      Glib::ustring album_artists_ustr = argv[i];



      // 
      new_track_sptr -> set_album_artists(new_track_sptr -> Multiple_Values_Tag_Decode(album_artists_ustr));

    }

    // 
    else if(strcmp(column_name[i], "ARTIST") == 0)
    {

      // 
      Glib::ustring artists_ustr = argv[i];



      // 
      new_track_sptr -> set_artists(new_track_sptr -> Multiple_Values_Tag_Decode(artists_ustr));

    } 

    // 
    else if(strcmp(column_name[i], "BIT_DEPTH") == 0)
    {

      // 
      new_track_sptr -> set_bit_depth(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "BIT_RATE") == 0)
    { 

      // 
      new_track_sptr -> set_bit_rate(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "CHANNELS") == 0)
    { 

      // 
      new_track_sptr -> set_channels(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "CODEC") == 0)
    {

      // 
      new_track_sptr -> set_codec(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "DATE") == 0)
    {

      // 
      new_track_sptr -> set_date(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "DISC_NUMBER") == 0)
    {

      // 
      new_track_sptr -> set_disc_number(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "DISC_TOTAL") == 0)
    {

      // 
      new_track_sptr -> set_disc_total(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "DURATION") == 0)
    {

      // 
      new_track_sptr -> set_duration(atol(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "END") == 0)
    { 

      // 
      new_track_sptr -> set_end(atol(argv[i]));

    }

    // 
    if(strcmp(column_name[i], "FILE_NAME") == 0)
    {

      // 
      new_track_sptr -> set_filename(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "GENRE") == 0)
    {

      // 
      Glib::ustring genres_ustr = argv[i];



      // 
      new_track_sptr -> set_genres(new_track_sptr -> Multiple_Values_Tag_Decode(genres_ustr));

    }

    // 
    else if(strcmp(column_name[i], "LENGTH_CS") == 0)
    {

      // 
      new_track_sptr -> set_length(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "MIME") == 0)
    {

      // 
      new_track_sptr -> set_mime(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "PREGAP_START") == 0)
    { 

      // 
      new_track_sptr -> set_pregap_start(atol(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "REPLAY_GAIN_ALBUM_GAIN") == 0)
    { 

      // 
      new_track_sptr -> set_replaygain_album_gain(atof(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "REPLAY_GAIN_ALBUM_PEAK") == 0)
    { 

      // 
      new_track_sptr -> set_replaygain_album_peak(atof(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "REPLAY_GAIN_TRACK_GAIN") == 0)
    { 

      // 
      new_track_sptr -> set_replaygain_track_gain(atof(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "REPLAY_GAIN_TRACK_PEAK") == 0)
    { 

      // 
      new_track_sptr -> set_replaygain_track_peak(atof(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "SAMPLE_RATE") == 0)
    { 

      // 
      new_track_sptr -> set_sample_rate(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "START") == 0)
    { 

      // 
      new_track_sptr -> set_start(atol(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "TITLE") == 0)
    {

      // 
      new_track_sptr -> set_title(argv[i]);

    }

    // 
    else if(strcmp(column_name[i], "TYPE") == 0)
    {

      // 
      TrackType type = TrackType(atoi(argv[i]));



      // 
      new_track_sptr -> set_type(type);

    }

    // 
    else if(strcmp(column_name[i], "TRACK_NUMBER") == 0)
    {

      // 
      new_track_sptr -> set_track_number(atoi(argv[i]));

    }

    // 
    else if(strcmp(column_name[i], "TRACK_TOTAL") == 0)
    {

      // 
      new_track_sptr -> set_track_total(atoi(argv[i]));

    }

  }



  // 
  ids_and_tracks_ptr -> push_back(make_pair(id, new_track_sptr));



  // 
  return 0;

}

Glib::ustring* PlaylistsDatabase::Multiple_Values_Tag_Encode
  (vector<Glib::ustring*>& tag)
{

  // 
  Glib::ustring* tags_ustr_ptr = new Glib::ustring;

  // 
  int count = 0;



  // 
  for(auto tag_it : tag)
  {

    // 
    if(count > 0)
    {

      // 
      (*tags_ustr_ptr) += "; ";

    }



    // 
    for(auto tag_it_char : *tag_it)
    {

      // 
      if(tag_it_char == ';')
      {

        // 
        (*tags_ustr_ptr) += "\\;";

      }

      // 
      else
      {

        // 
        (*tags_ustr_ptr) += tag_it_char;

      }

    }



    // 
    count++;

  }



  // 
  return tags_ustr_ptr;

}

bool PlaylistsDatabase::Playlist_Names(std::vector<std::string>& playlist_names)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  char* sql;  // Create SQL statement



  // Create merged SQL statement 
  sql = "SELECT name FROM sqlite_master WHERE type='table';";



  // Execute SQL statement
  result_code = sqlite3_exec(database_, sql, Playlist_Names_Callback,
                             &playlist_names, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

//    fprintf(stderr, "SQL error: %s\n", error_message);

    sqlite3_free(error_message);

    return false;

  }

  // 
  else
  {


    return true;

  }
  

}

int PlaylistsDatabase::Playlist_Names_Callback
  (void* names, int argc, char **argv, char **azColName)
{

  int i;



  vector<string>* names_ptr = (vector<string>*)(names);


  for(i = 0; i < argc; i++)
  {

//      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");

    names_ptr -> push_back(string(argv[i]));

  }


  return 0;

}

bool PlaylistsDatabase::Rebuild_Database()
{

  // 
  if((!(mutex_ . try_lock())) && (!(playlists() . rebuilding_databases())))
  {

    // 
    return false;

  }



  // 
  while(!(mutex_ . try_lock()))
  {

    // 
    string copy_database_str
      = "cp " + (*database_ustr_ptr_) + "/playlists.db "
          + (*database_ustr_ptr_) + "/playlists.db.backup";

    // 
    system(copy_database_str . c_str());



    // 
    mutex_ . unlock();  

  }



  // 
  playlists() . rebuilding_databases() = true;

  // 
  playlists() . rebuild_databases() = false;



  // 
  static auto playlist_treestores_it
    = playlists() . playlist_treestores() . begin();

  // 
  playlist_treestores_it
    = playlists() . playlist_treestores() . begin();

  // 
  while(true)
  {

    // 
    if(playlist_treestores_it == (playlists() . playlist_treestores() . end()))
    {

      // 
      mutex_ . unlock();



      // 
      playlists() . rebuilding_databases() = false;



      // 
      return false;

    }

    // 
    if((*playlist_treestores_it) -> rebuild_database())
    {

      // 
      break;

    }



    // 
    playlist_treestores_it++;

  }



  // 
  static auto playlist_row_it
    = (*playlist_treestores_it) -> children() . begin();

  // 
  playlist_row_it = (*playlist_treestores_it) -> children() . begin();

  // 
  static string sql;

  // 
  static string sql_part;

  sql_part = "";

  // 
  static int id;

  // 
  id = 0;

  // 
  static int track_total_int;

  // 
  track_total_int = 0;

  // 
  static int current_track_int;

  // 
  current_track_int = 0;

  // 
  static atomic<bool> database_saving_thread_active = false;

  // 
  database_saving_thread_active = false;

  // 
  static atomic<bool> database_saving_thread_finished;

  // 
  database_saving_thread_finished = false;

  // 
  static double database_saving_pos = 0.00;

  // 
  database_saving_pos = 0.00;

  // 
  static bool database_saving_inverted = false;

  database_saving_inverted = false;

  // 
  static bool database_saving_decreasing = false;

  database_saving_decreasing = false;



  // 
  string playlist_name_str = (*playlist_treestores_it) -> get_name();

  // 
  const char* playlist_name = playlist_name_str . c_str();

  // 
  Clear_Playlist(playlist_name);



  // 
  for(auto playlist_treestore : playlists() . playlist_treestores())
  {

    // 
    if(playlist_treestore -> rebuild_database())
    {

      // 
      track_total_int += playlist_treestore -> children() . size();

      // 
      playlist_treestore -> rebuild_scheduled() = true;

      // 
      playlist_treestore -> restart_changes() = false;

      // 
      playlist_treestore -> cancel_changes() = false;

    }

  }



  // 
  if(track_total_int < 1)
  {

    // 
    return false;

  }



  // 
  sigc::connection program_conn = Glib::signal_timeout() . connect
  (

    // 
    [this]() -> bool
    {

      // 
      if(database_saving_thread_active)
      {

        // 
        if(database_saving_thread_finished)
        {

          // 
          database_saving_thread_active = false;



          // 
          for(auto playlists_it : playlists()())
          {

            // 
            playlists_it -> progress_bar()
              . set_text("No Playlist Modifications Occurring");

            // 
            playlists_it -> progress_bar() . set_fraction(1);

            // 
            playlists_it -> progress_bar() . set_inverted(false);

          }



          // 
          mutex_ . unlock();



          // 
          playlists() . rebuilding_databases() = false;



          // 
          if(playlists() . rebuild_databases())
          {

            // 
            Rebuild_Database();

          }



          // 
          return false;

        }

        // 
        else
        {

          // 
          if(database_saving_pos >= double(1.000))
          {

            //
            database_saving_inverted = !database_saving_inverted;



            // 
            database_saving_decreasing = true;



            // 
            database_saving_pos = 1;

          }

          else if(database_saving_pos <= double(0.000))
          {

            // 
            database_saving_decreasing = false;



            // 
            database_saving_pos = 0;

          }



          // 
          for(auto playlists_it : playlists()())
          {

            // 
            playlists_it -> progress_bar()
              . set_inverted(database_saving_inverted);


            // 
            playlists_it -> progress_bar()
              . set_text("Saving To Playlists Database");



            // 
            playlists_it -> progress_bar() . set_fraction(database_saving_pos);

          }



          // 
          if(database_saving_decreasing)
          {

            // 
            database_saving_pos -= 0.005;

          }

          // 
          else
          {

            // 
            database_saving_pos += 0.005;

          }



          // 
          return true;

        }

      }



      //  
      if(base() . quitting() || quit_rebuilding_)
      {

        // 
        if(base() . quitting())
        {

          // 
          sqlite3_close(database_);



          // 
          string copy_database_str
            = "cp " + (*database_ustr_ptr_) + "/playlists.db.backup "
                + (*database_ustr_ptr_) + "/playlists.db";

          // 
          system(copy_database_str . c_str());

        }



        // 
        mutex_ . unlock();



        // 
        sql = "";



        // 
        (*playlist_treestores_it) -> rebuild_database() = true;

        // 
        quit_rebuilding_ = false;

        // 
        playlists() . rebuilding_databases() = false;

        // 
        database_saving_thread_active = false;

        // 
        database_saving_thread_finished = false;



        // 
        mutex_ . unlock();



        // 
        return false;

      }



      // 
      if((*playlist_treestores_it) -> appending())
      {

        // 
        return true;

      }

      // 
      else if((*playlist_treestores_it) -> pause_changes())
      {

        // 
        return true;

      }

      // 
      if((*playlist_treestores_it) -> restart_changes())
      {

        // 
        (*playlist_treestores_it) -> restart_changes() = false;

        // 
        playlist_row_it = (*playlist_treestores_it) -> children() . begin();



        // 
        current_track_int -= id;

        // 
        track_total_int = (*playlist_treestores_it) -> children() . size();



        // 
        sql_part = "";

        // 
        id = 0;



        // 
        return true;

      }



      // 
      if(!playlist_row_it)
      {

        // 
        sql += sql_part;

        // 
        sql_part = "";



        // 
        (*playlist_treestores_it) -> rebuild_database() = false;

        // 
        (*playlist_treestores_it) -> rebuilding_database() = false;



        // 
        playlist_treestores_it++;



        // 
        while(true)
        {

          // 
          if(playlist_treestores_it
               == playlists() . playlist_treestores() . end())
          {

            // 
            database_saving_thread_active = true;

            // 
            database_saving_thread_finished = false;



            // 
            std::thread database_saving_thread
            (
    
              [this]()
              {

                // 
                char* error_message = 0;

                // 
                int result_code;



                // 
                result_code = sqlite3_exec(database_, sql . c_str(), 0,
                                           0, &error_message);



                //    
                if(result_code != SQLITE_OK )
                {

                  stringstream debug_ss;

                  debug_ss << "SQL error: " << error_message;

                  debug(debug_ss . str() . c_str());



                  // 
                  sqlite3_free(error_message);

                }

                //
                else
                {

                  // 
                  sql_part = "";

                  // 
                  sql = "";

                }



                // 
                database_saving_thread_finished = true;

              }

            );

            // 
            database_saving_thread . detach();



            // 
            return true;

          }

          else if(((*playlist_treestores_it) -> rebuild_database())
                    && ((*playlist_treestores_it) -> rebuild_scheduled()))
          {

            // 
            string playlist_name_str = (*playlist_treestores_it) -> get_name();

            // 
            const char* playlist_name = playlist_name_str . c_str();

            // 
            Clear_Playlist(playlist_name);



            // 
            id = 0;

            // 
            playlist_row_it = (*playlist_treestores_it) -> children() . begin();

            // 
            (*playlist_treestores_it) -> rebuild_scheduled() = false;

            // 
            (*playlist_treestores_it) -> rebuilding_database() = false;



            // 
            return true;

          }



          // 
          playlist_treestores_it++;

        }

      } 



      // 
      (*playlist_treestores_it) -> rebuilding_database() = true;



      // 
      string playlist_name_str = (*playlist_treestores_it) -> get_name();

      // 
      const char* playlist_name = playlist_name_str . c_str();



      // 
      Add_Column(playlist_name, "DISC_NUMBER", "INT");

      // 
      Add_Column(playlist_name, "DISC_TOTAL", "INT");



      // 
      Gtk::TreeRow temp_treerow = *playlist_row_it;

      // 
      shared_ptr<Track> track_sptr
        = temp_treerow[playlists() . playlist_column_record() . track_];



      // 
      temp_treerow[playlists() . playlist_column_record() . id_] = id;



      // 
      Glib::ustring* artists_str_ptr
        = Multiple_Values_Tag_Encode(track_sptr -> artists());

      // 
      Glib::ustring* album_artists_str_ptr
        = Multiple_Values_Tag_Encode(track_sptr -> album_artists());

      // 
      Glib::ustring* genres_str_ptr
        = Multiple_Values_Tag_Encode(track_sptr -> genres());



      // 
      sql_part += "INSERT INTO '";

      // 
      sql_part += Convert(playlist_name);

      // 
      sql_part += "' (ID, ALBUM, ALBUM_ARTIST, ARTIST, BIT_DEPTH, BIT_RATE, CHANNELS, CODEC, " \
                  "DATE, DISC_NUMBER, DISC_TOTAL, DURATION, END, FILE_NAME, GENRE, LENGTH_CS, MIME, PREGAP_START, " \
                  "REPLAY_GAIN_ALBUM_GAIN, REPLAY_GAIN_ALBUM_PEAK, " \
                  "REPLAY_GAIN_TRACK_GAIN, REPLAY_GAIN_TRACK_PEAK, " \
                  "SAMPLE_RATE, START, TITLE, TRACK_NUMBER, TRACK_TOTAL, " \
                  "TYPE) " \
                  "VALUES (" + to_string(id) + ", '"
                  + Convert(track_sptr -> album()) + "', '"
                  + Convert(album_artists_str_ptr -> raw()) + "', '"
                  + Convert(artists_str_ptr -> raw()) + "', "
                  + to_string(track_sptr -> bit_depth()) + ", "
                  + to_string(track_sptr -> bit_rate()) + ", "
                  + to_string(track_sptr -> channels()) + ", '"
                  + Convert(track_sptr -> codec()) + "', "
                  + to_string(track_sptr -> date()) + ", "
                  + to_string(track_sptr -> disc_number()) + ", "
                  + to_string(track_sptr -> disc_total()) + ", "
                  + to_string(track_sptr -> duration()) + ", "
                  + to_string(track_sptr -> end()) + ", '"
                  + Convert(track_sptr -> filename()) + "', '"
                  + Convert(genres_str_ptr -> raw()) + "', '"
                  + Convert(track_sptr -> length()) + "', '"
                  + Convert(track_sptr -> mime()) + "', "
                  + to_string(track_sptr -> pregap_start()) + ", "
                  + to_string(track_sptr -> replaygain_album_gain()) + ", "
                  + to_string(track_sptr -> replaygain_album_peak()) + ", "
                  + to_string(track_sptr -> replaygain_track_gain()) + ", "
                  + to_string(track_sptr -> replaygain_track_peak()) + ", "
                  + to_string(track_sptr -> sample_rate()) + ", "
                  + to_string(track_sptr -> start()) + ", '"
                  + Convert(track_sptr -> title()) + "', "
                  + to_string(track_sptr -> track_number()) + ", "
                  + to_string(track_sptr -> track_total()) + ", "
                  + to_string(int(track_sptr -> type())) + ");";



      // 
      delete artists_str_ptr;

      // 
      delete genres_str_ptr;



      // 
      id++;

      // 
      playlist_row_it++;

      // 
      current_track_int++;


      
      //
      string playlist_status
        = "Collecting Track Data For Database Rebuild: "
        + to_string(current_track_int) + " / " + to_string(track_total_int);

      // 
      double completion_fraction
        = current_track_int / double(track_total_int);



      // 
      for(auto playlists_it : playlists()())
      {

        // 
        playlists_it -> progress_bar() . set_text(playlist_status);

        // 
        playlists_it -> progress_bar() . set_fraction(completion_fraction);

      }



      // 
      return true;

    },



    // 
    3, Glib::PRIORITY_HIGH_IDLE

  );



  // 
  return false;

}

bool PlaylistsDatabase::Rename_Playlist
  (const char* playlist_name, const char* new_playlist_name)
{

  // 
  char* error_message = 0;

  // 
  int result_code;

  // 
  string sql = "";



  // 
  sql = "ALTER TABLE \'";

  // 
  sql += Convert(playlist_name);

  // 
  sql += "\' RENAME TO \'";

  sql += Convert(new_playlist_name);

  sql += "\';";



  // 
  result_code
  = sqlite3_exec(database_, sql . c_str(), 0, 0, &error_message);



  //    
  if(result_code != SQLITE_OK )
  {

    stringstream debug_ss;

    debug_ss << "SQL error: " << error_message;

    debug(debug_ss . str() . c_str());



    // 
    sqlite3_free(error_message);

    // 
    return false;

  }

  // 
  else
  {

    // 
    return true;

  }

}    





//         //
//         //
// Getters ////////////////////////////////////////////////////////////////////
//         //
//         //

std::atomic<bool>& PlaylistsDatabase::quit_rebuilding()
{

  // 
  return quit_rebuilding_;

}
