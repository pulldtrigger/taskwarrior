////////////////////////////////////////////////////////////////////////////////
// taskwarrior - a command line task list manager.
//
// Copyright 2006 - 2011, Paul Beckingham, Federico Hernandez.
// All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the
//
//     Free Software Foundation, Inc.,
//     51 Franklin Street, Fifth Floor,
//     Boston, MA
//     02110-1301
//     USA
//
////////////////////////////////////////////////////////////////////////////////

#include <iostream> // TODO Remove.
#include <Context.h>
#include <text.h>
#include <TDB2.h>

extern Context context;

////////////////////////////////////////////////////////////////////////////////
TF2::TF2 ()
: _read_only (false)
, _dirty (false)
, _loaded_tasks (false)
, _loaded_lines (false)
, _loaded_contents (false)
, _contents ("")
{
}

////////////////////////////////////////////////////////////////////////////////
TF2::~TF2 ()
{
}

////////////////////////////////////////////////////////////////////////////////
void TF2::target (const std::string& f)
{
  _file = File (f);
  _read_only = ! _file.writable ();

//  std::cout << "# TF2::target " << f << "\n";
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <Task>& TF2::get_tasks ()
{
//  std::cout << "# TF2::get_tasks\n";

  if (! _loaded_tasks)
    load_tasks ();

  return _tasks;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <std::string>& TF2::get_lines ()
{
//  std::cout << "# TF2::get_lines\n";

  if (! _loaded_lines)
    load_lines ();

  return _lines;
}

////////////////////////////////////////////////////////////////////////////////
const std::string& TF2::get_contents ()
{
//  std::cout << "# TF2::get_contents\n";

  if (! _loaded_contents)
    load_contents ();

  return _contents;
}

////////////////////////////////////////////////////////////////////////////////
void TF2::add_task (const Task& task)
{
//  std::cout << "# TF2::add_task\n";

  _tasks.push_back (task);           // For subsequent queries
  _added_tasks.push_back (task);     // For commit/synch
  _dirty = true;
}

////////////////////////////////////////////////////////////////////////////////
void TF2::modify_task (const Task& task)
{
//  std::cout << "# TF2::modify_task\n";

  // Modify in-place.
  std::vector <Task>::iterator i;
  for (i = _tasks.begin (); i != _tasks.end (); ++i)
  {
    if (i->get ("uuid") == task.get ("uuid"))
    {
      *i = task;
      break;
    }
  }

  _modified_tasks.push_back (task);
  _dirty = true;
}

////////////////////////////////////////////////////////////////////////////////
void TF2::add_line (const std::string& line)
{
//  std::cout << "# TF2::add_line\n";

  _added_lines.push_back (line);
  _dirty = true;
}

////////////////////////////////////////////////////////////////////////////////
// This is so that synch.key can just overwrite and not grow.
void TF2::clear_lines ()
{
//  std::cout << "# TF2::clear_lines\n";
  _lines.clear ();
  _dirty = true;
}

////////////////////////////////////////////////////////////////////////////////
// Top-down recomposition.
void TF2::commit ()
{
//  std::cout << "# TF2::commit " << _file.data << "\n";

  // The _dirty flag indicates that the file needs to be written.
  if (_dirty)
  {
    // Special case: added but no modified means just append to the file.
    if (!_modified_tasks.size () &&
        (_added_tasks.size () || _added_lines.size ()))
    {
      if (_file.open ())
      {
        if (context.config.getBoolean ("locking"))
          _file.lock ();

        // Write out all the added tasks.
        std::vector <Task>::iterator task;
        for (task = _added_tasks.begin ();
             task != _added_tasks.end ();
             ++task)
        {
          _file.append (task->composeF4 ());
        }

        _added_tasks.clear ();

        // Write out all the added lines.
        std::vector <std::string>::iterator line;
        for (line = _added_lines.begin ();
             line != _added_lines.end ();
             ++line)
        {
          _file.append (*line);
        }

        _added_lines.clear ();
        _file.close ();
      }
    }
    else
    {
      // TODO _file.truncate ();
      // TODO only write out _tasks, because any deltas have already been applied.
      // TODO append _added_lines.
    }

    _dirty = false;
  }


  // --------------------------- old implementation -------------------------
/*
  // Load the lowest form, to allow
  if (_dirty)
  {
    load_contents ();

    if (_modified_tasks.size ())
    {
      std::map <std::string, Task> modified;
      std::vector <Task>::iterator it;
      for (it = _modified_tasks.begin (); it != _modified_tasks.end (); ++it)
        modified[it->get ("uuid")] = *it;

//    for (it = _

     _modified_tasks.clear ();
    }

    if (_added_tasks.size ())
    {
      std::vector <Task>::iterator it;
      for (it = _added_tasks.begin (); it != _added_tasks.end (); ++it)
        _lines.push_back (it->composeF4 ());

      _added_tasks.clear ();
    }

    if (_added_lines.size ())
    {
      //_lines += _added_lines;
      _added_lines.clear ();
    }

// TODO This clobbers minimal case.

    _contents = "";  // TODO Verify no resize.
    join (_contents, "\n", _lines);
    _file.write (_contents);

    _dirty = false;
  }
*/
}

////////////////////////////////////////////////////////////////////////////////
void TF2::load_tasks ()
{
//  std::cout << "# TF2::load_tasks\n";

  if (! _loaded_lines)
    load_lines ();

  int line_number = 0;
  try
  {
    std::vector <std::string>::iterator i;
    for (i = _lines.begin (); i != _lines.end (); ++i)
    {
      ++line_number;
      Task task (*i);
// TODO Find a way to number pending tasks, but not others.
//      task.id = _id++;
      _tasks.push_back (task);
    }

    _loaded_tasks = true;
  }

  catch (std::string& e)
  {
/*
    std::stringstream s;
    s << " in " << _file.data << " at line " << line_number;
    throw e + s.str ();
*/
    throw e + format (" in {1} at line {2}", _file.data, line_number);
  }
}

////////////////////////////////////////////////////////////////////////////////
void TF2::load_lines ()
{
//  std::cout << "# TF2::load_lines\n";

  if (! _loaded_contents)
    load_contents ();

  split (_lines, _contents, '\n');
  _loaded_lines = true;
}

////////////////////////////////////////////////////////////////////////////////
void TF2::load_contents ()
{
//  std::cout << "# TF2::load_contents\n";

  _contents = "";

  if (_file.open ())
  {
    if (context.config.getBoolean ("locking"))
      _file.lock ();

    _file.read (_contents);
    _loaded_contents = true;
  }
  // TODO Error handling?
}

////////////////////////////////////////////////////////////////////////////////









////////////////////////////////////////////////////////////////////////////////
TDB2::TDB2 ()
: _location ("")
, _id (1)
{
}

////////////////////////////////////////////////////////////////////////////////
// Deliberately no file writes on destruct.  TDB2::commit should have been
// already called, if data is to be preserved.
TDB2::~TDB2 ()
{
}

////////////////////////////////////////////////////////////////////////////////
// Once a location is known, the files can be set up.  Note that they are not
// read.
void TDB2::set_location (const std::string& location)
{
//  std::cout << "# TDB2::set_location " << location << "\n";
  _location = location;

  pending.target   (location + "/pending.data");
  completed.target (location + "/completed.data");
  undo.target      (location + "/undo.data");
  backlog.target   (location + "/backlog.data");
  synch_key.target (location + "/synch.key");
}

////////////////////////////////////////////////////////////////////////////////
// Add the new task to the appropriate file.
void TDB2::add (const Task& task)
{
//  std::cout << "# TDB2::add\n";

  std::string status = task.get ("status");
  if (status == "completed" ||
      status == "deleted")
    completed.add_task (task);
  else
    pending.add_task (task);

  backlog.add_task (task);
}

////////////////////////////////////////////////////////////////////////////////
void TDB2::modify (const Task& task)
{
//  std::cout << "# TDB2::modify\n";

  std::string status = task.get ("status");
  if (status == "completed" ||
      status == "deleted")
    completed.modify_task (task);
  else
    pending.modify_task (task);

  backlog.modify_task (task);
}

////////////////////////////////////////////////////////////////////////////////
void TDB2::commit ()
{
  dump ();
//  std::cout << "# TDB2::commit\n";
  pending.commit ();
  completed.commit ();
  undo.commit ();
  backlog.commit ();
  synch_key.commit ();
  dump ();
}

////////////////////////////////////////////////////////////////////////////////
// Scans the pending tasks for any that are completed or deleted, and if so,
// moves them to the completed.data file.  Returns a count of tasks moved.
// Now reverts expired waiting tasks to pending.
// Now cleans up dangling dependencies.
int TDB2::gc ()
{
//  std::cout << "# TDB2::gc\n";
/*
  pending.load_tasks
  completed.load_tasks

  for each pending
    if status == completed || status == deleted
      pending.remove
      completed.add
    if status == waiting && wait < now
      status = pending
      wait.clear

  for each completed
    if status == pending || status == waiting
      completed.remove
      pending.add
*/

  // TODO Remove dangling dependencies
  // TODO Wake up expired waiting tasks

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
int TDB2::next_id ()
{
  return _id++;
}

////////////////////////////////////////////////////////////////////////////////
//
//  File           RW State Tasks + - ~ lines + - Bytes
//  -------------- -- ----- ----- - - - ----- - - -----
//  pending.data   rw clean   123t  +2t  -1t ~1t
//  completed.data rw clean   123t  +2t      ~1t
//  undo.data      rw clean   123t  +2t      ~1t
//  backlog.data   rw clean   123t  +2t      ~1t
//  synch-key.data rw clean                        123b
//
void TDB2::dump ()
{
  if (context.config.getBoolean ("debug"))
  {
    ViewText view;
    view.width (context.getWidth ());
    view.add (Column::factory ("string",       "File"));
    view.add (Column::factory ("string.right", "RW"));
    view.add (Column::factory ("string.right", "State"));
    view.add (Column::factory ("string.right", "Tasks"));
    view.add (Column::factory ("string.right", "+"));
    view.add (Column::factory ("string.right", "~"));
    view.add (Column::factory ("string.right", "Lines"));
    view.add (Column::factory ("string.right", "+"));
    view.add (Column::factory ("string.right", "Bytes"));

    dump_file (view, "pending.data", pending);
    dump_file (view, "completed.data", completed);
    dump_file (view, "undo.data", undo);
    dump_file (view, "backlog.data", backlog);
    dump_file (view, "synch_key.data", synch_key);
    context.debug (view.render ());
  }
}

////////////////////////////////////////////////////////////////////////////////
void TDB2::dump_file (ViewText& view, const std::string& label, TF2& tf)
{
  int row = view.addRow ();
  view.set (row, 0, label);
  view.set (row, 1, std::string (tf._file.readable () ? "r" : "-") +
                    std::string (tf._file.writable () ? "w" : "-"));
  view.set (row, 2, tf._dirty ? "dirty" : "clean");
  view.set (row, 3, tf._loaded_tasks ? (format ((int)tf._tasks.size ())) : "-");
  view.set (row, 4, (int)tf._added_tasks.size ());
  view.set (row, 5, (int)tf._modified_tasks.size ());
  view.set (row, 6, tf._loaded_lines ? (format ((int)tf._lines.size ())) : "-");
  view.set (row, 7, (int)tf._added_lines.size ());
  view.set (row, 8, tf._loaded_contents ? (format ((int)tf._contents.size ())) : "-");
}

////////////////////////////////////////////////////////////////////////////////

#if 0
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <list>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <stdlib.h>
#include <text.h>
#include <util.h>
#include <TDB.h>
#include <Directory.h>
#include <File.h>
#include <ViewText.h>
#include <Timer.h>
#include <Color.h>
#include <main.h>

#define NDEBUG
#include <assert.h>
#include <Taskmod.h>

#define DEBUG_OUTPUT 0

#if DEBUG_OUTPUT > 0
  #define DEBUG_STR(str)       std::cout << "DEBUG: " << str << "\n"; std::cout.flush()
  #define DEBUG_STR_PART(str)  std::cout << "DEBUG: " << str; std::cout.flush()
  #define DEBUG_STR_END(str)   std::cout << str << "\n"; std::cout.flush()
#else
  #define DEBUG_STR(str)
  #define DEBUG_STR_PART(str)
  #define DEBUG_STR_END(str)
#endif

extern Context context;

////////////////////////////////////////////////////////////////////////////////
// Helper function for TDB::merge
void readTaskmods (std::vector <std::string> &input,
                   std::vector <std::string>::iterator &start,
                   std::list<Taskmod> &list)
{
  std::string  line;
  Taskmod      tmod_tmp;

  DEBUG_STR ("reading taskmods from file: ");

  for ( ; start != input.end (); ++start)
  {
    line = *start;

    if (line.substr (0, 4) == "time")
    {
      std::stringstream stream (line.substr (5));
      long ts;
      stream >> ts;

      if (stream.fail ())
        throw std::string ("There was a problem reading the timestamp from the undo.data file.");

      // 'time' is the first line of a modification
      // thus we will (re)set the taskmod object
      tmod_tmp.reset (ts);

    }
    else if (line.substr (0, 3) == "old")
    {
      tmod_tmp.setBefore (Task (line.substr (4)));

    }
    else if (line.substr (0, 3) == "new")
    {
      tmod_tmp.setAfter (Task (line.substr (4)));

      // 'new' is the last line of a modification,
      // thus we can push to the list
      list.push_back (tmod_tmp);

      assert (tmod_tmp.isValid ());
      DEBUG_STR ("  taskmod complete");
    }
  }

  DEBUG_STR ("DONE");
}

////////////////////////////////////////////////////////////////////////////////
//  The ctor/dtor do nothing.
//  The lock/unlock methods hold the file open.
//  There should be only one commit.
//
//  +- TDB::TDB
//  |
//  |  +- TDB::lock
//  |  |    open
//  |  |    [lock]
//  |  |
//  |  |  +- TDB::load
//  |  |  |    read all
//  |  |  |    apply filter
//  |  |  |    return subset
//  |  |  |
//  |  |  +- TDB::add (T)
//  |  |  |
//  |  |  +- TDB::update (T)
//  |  |  |
//  |  |  +- TDB::commit
//  |  |  |   write all
//  |  |  |
//  |  |  +- TDB::undo
//  |  |
//  |  +- TDB::unlock
//  |       [unlock]
//  |       close
//  |
//  +- TDB::~TDB
//       [TDB::unlock]
//
TDB::TDB ()
: mLock (true)
, mAllOpenAndLocked (false)
, mId (1)
{
}

////////////////////////////////////////////////////////////////////////////////
TDB::~TDB ()
{
  if (mAllOpenAndLocked)
    unlock ();
}

////////////////////////////////////////////////////////////////////////////////
void TDB::clear ()
{
  mLocations.clear ();
  mLock = true;

  if (mAllOpenAndLocked)
    unlock ();

  mAllOpenAndLocked = false;
  mId = 1;
  mPending.clear ();
  mNew.clear ();
  mCompleted.clear ();
  mModified.clear ();
}

////////////////////////////////////////////////////////////////////////////////
void TDB::location (const std::string& path)
{
  Directory d (path);
  if (!d.exists ())
    throw std::string ("Data location '") +
          path +
          "' does not exist, or is not readable and writable.";

  mLocations.push_back (Location (d));
}

////////////////////////////////////////////////////////////////////////////////
void TDB::lock (bool lockFile /* = true */)
{
  mLock = lockFile;

  mPending.clear ();
  mNew.clear ();
  mCompleted.clear ();
  mModified.clear ();

  foreach (location, mLocations)
  {
    location->pending   = openAndLock (location->path + "/pending.data");
    location->completed = openAndLock (location->path + "/completed.data");
    location->undo      = openAndLock (location->path + "/undo.data");
  }

  mAllOpenAndLocked = true;
}

////////////////////////////////////////////////////////////////////////////////
void TDB::unlock ()
{
  // Do not clear out these items, as they may be used in a read-only fashion.
  // mPending.clear ();
  // mNew.clear ();
  // mModified.clear ();

  foreach (location, mLocations)
  {
    fflush (location->pending);
    fclose (location->pending);
    location->pending = NULL;

    fflush (location->completed);
    fclose (location->completed);
    location->completed = NULL;

    fflush (location->undo);
    fclose (location->undo);
    location->completed = NULL;
  }

  mAllOpenAndLocked = false;
}

////////////////////////////////////////////////////////////////////////////////
// Returns number of filtered tasks.
// Note: tasks.clear () is deliberately not called, to allow the combination of
//       multiple files.
int TDB::load (std::vector <Task>& tasks, Filter& filter)
{
  // Special optimization: if the filter contains Att ('status', '', 'pending'),
  // and no other 'status' filters, then loadCompleted can be skipped.
  int numberStatusClauses = 0;
  int numberSimpleStatusClauses = 0;
  foreach (att, filter)
  {
    if (att->name () == "status")
    {
      ++numberStatusClauses;

      if (att->mod () == "" &&
          (att->value () == "pending" ||
           att->value () == "waiting"))
        ++numberSimpleStatusClauses;
    }
  }

  loadPending (tasks, filter);

  if (numberStatusClauses == 0 ||
      numberStatusClauses != numberSimpleStatusClauses)
    loadCompleted (tasks, filter);
  else
    context.debug ("load optimization short circuit");

  return tasks.size ();
}

////////////////////////////////////////////////////////////////////////////////
// Returns number of filtered tasks.
// Note: tasks.clear () is deliberately not called, to allow the combination of
//       multiple files.
int TDB::loadPending (std::vector <Task>& tasks, Filter& filter)
{
  Timer t ("TDB::loadPending");

  std::string file;
  int line_number = 1;

  try
  {
    // Only load if not already loaded.
    if (mPending.size () == 0)
    {
      mId = 1;
      char line[T_LINE_MAX];
      foreach (location, mLocations)
      {
        line_number = 1;
        file = location->path + "/pending.data";

        fseek (location->pending, 0, SEEK_SET);
        while (fgets (line, T_LINE_MAX, location->pending))
        {
          int length = strlen (line);
          if (length > 3) // []\n
          {
            // TODO Add hidden attribute indicating source?
            Task task (line);

            Task::status status = task.getStatus ();
            task.id = mId++;

            mPending.push_back (task);

            // Maintain mapping for ease of link/dependency resolution.
            // Note that this mapping is not restricted by the filter, and is
            // therefore a complete set.
            mI2U[task.id] = task.get ("uuid");
            mU2I[task.get ("uuid")] = task.id;
          }

          ++line_number;
        }
      }
    }

    // Now filter and return.
    if (filter.size ())
    {
      foreach (task, mPending)
        if (filter.pass (*task))
          tasks.push_back (*task);
    }
    else
    {
      foreach (task, mPending)
        tasks.push_back (*task);
    }

    // Hand back any accumulated additions, if TDB::loadPending is being called
    // repeatedly.
    int fakeId = mId;
    if (filter.size ())
    {
      foreach (task, mNew)
      {
        task->id = fakeId++;
        if (filter.pass (*task))
          tasks.push_back (*task);
      }
    }
    else
    {
      foreach (task, mNew)
      {
        task->id = fakeId++;
        tasks.push_back (*task);
      }
    }
  }

  catch (std::string& e)
  {
    std::stringstream s;
    s << " in " << file << " at line " << line_number;
    throw e + s.str ();
  }

  return tasks.size ();
}

////////////////////////////////////////////////////////////////////////////////
// Returns number of filtered tasks.
// Note: tasks.clear () is deliberately not called, to allow the combination of
//       multiple files.
int TDB::loadCompleted (std::vector <Task>& tasks, Filter& filter)
{
  Timer t ("TDB::loadCompleted");

  std::string file;
  int line_number = 1;

  try
  {
    if (mCompleted.size () == 0)
    {
      char line[T_LINE_MAX];
      foreach (location, mLocations)
      {
        line_number = 1;
        file = location->path + "/completed.data";

        fseek (location->completed, 0, SEEK_SET);
        while (fgets (line, T_LINE_MAX, location->completed))
        {
          int length = strlen (line);
          if (length > 3) // []\n
          {
            // TODO Add hidden attribute indicating source?

            Task task (line);
            task.id = 0;  // Need a value, just not a valid value.

            mCompleted.push_back (task);
          }

          ++line_number;
        }
      }
    }

    // Now filter and return.
    if (filter.size ())
    {
      foreach (task, mCompleted)
        if (filter.pass (*task))
          tasks.push_back (*task);
    }
    else
    {
      foreach (task, mCompleted)
        tasks.push_back (*task);
    }
  }

  catch (std::string& e)
  {
    std::stringstream s;
    s << " in " << file << " at line " << line_number;
    throw e + s.str ();
  }

  return tasks.size ();
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <Task>& TDB::getAllPending ()
{
  return mPending;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <Task>& TDB::getAllNew ()
{
  return mNew;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <Task>& TDB::getAllCompleted ()
{
  return mCompleted;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <Task>& TDB::getAllModified ()
{
  return mModified;
}

////////////////////////////////////////////////////////////////////////////////
// Note: mLocations[0] is where all tasks are written.
void TDB::add (const Task& task)
{
  std::string unique;
  Task t (task);
  if (task.get ("uuid") == "")
    unique = ::uuid ();
  else
    unique = task.get ("uuid");

  t.set ("uuid", unique);

  // If the tasks are loaded, then verify that this uuid is not already in
  // the file.
  if (uuidAlreadyUsed (unique, mNew)      ||
      uuidAlreadyUsed (unique, mModified) ||
      uuidAlreadyUsed (unique, mPending)  ||
      uuidAlreadyUsed (unique, mCompleted))
    throw std::string ("Cannot add task because the uuid '") + unique + "' is not unique.";

  mNew.push_back (t);
  mI2U[task.id] = unique;
  mU2I[task.get ("uuid")] = t.id;
}

////////////////////////////////////////////////////////////////////////////////
void TDB::update (const Task& task)
{
  mModified.push_back (task);
}

////////////////////////////////////////////////////////////////////////////////
// Interestingly, only the pending file gets written to.  The completed file is
// only modified by TDB::gc.
int TDB::commit ()
{
  Timer t ("TDB::commit");

  int quantity = mNew.size () + mModified.size ();

  // This is an optimization.  If there are only new tasks, and none were
  // modified, simply seek to the end of pending and write.
  if (mNew.size () && ! mModified.size ())
  {
    fseek (mLocations[0].pending, 0, SEEK_END);
    foreach (task, mNew)
    {
      mPending.push_back (*task);
      fputs (task->composeF4 ().c_str (), mLocations[0].pending);
    }

    fseek (mLocations[0].undo, 0, SEEK_END);
    foreach (task, mNew)
      writeUndo (*task, mLocations[0].undo);

    mNew.clear ();
    return quantity;
  }

  // The alternative is to potentially rewrite both files.
  else if (mNew.size () || mModified.size ())
  {
    // allPending is a copy of mPending, with all modifications included, and
    // new tasks appended.
    std::vector <Task> allPending;
    allPending = mPending;
    foreach (mtask, mModified)
    {
      foreach (task, allPending)
      {
        if (task->id == mtask->id)
        {
          *task = *mtask;
          goto next_mod;
        }
      }

    next_mod:
      ;
    }

    foreach (task, mNew)
      allPending.push_back (*task);

    // Write out all pending.
    if (fseek (mLocations[0].pending, 0, SEEK_SET) == 0)
    {
      if (ftruncate (fileno (mLocations[0].pending), 0))
        throw std::string ("Failed to truncate pending.data file ");

      foreach (task, allPending)
        fputs (task->composeF4 ().c_str (), mLocations[0].pending);
    }

    // Update the undo log.
    if (fseek (mLocations[0].undo, 0, SEEK_END) == 0)
    {
      foreach (mtask, mModified)
      {
        foreach (task, mPending)
        {
          if (task->id == mtask->id)
          {
            writeUndo (*task, *mtask, mLocations[0].undo);
            goto next_mod2;
          }
        }

      next_mod2:
        ;
      }

      foreach (task, mNew)
        writeUndo (*task, mLocations[0].undo);
    }

    mPending = allPending;

    mModified.clear ();
    mNew.clear ();
  }

  return quantity;
}

////////////////////////////////////////////////////////////////////////////////
// Scans the pending tasks for any that are completed or deleted, and if so,
// moves them to the completed.data file.  Returns a count of tasks moved.
// Now reverts expired waiting tasks to pending.
// Now cleans up dangling dependencies.
int TDB::gc ()
{
  Timer t ("TDB::gc");

  // Allowed as a temporary override.
  if (!context.config.getBoolean ("gc"))
    return 0;

  int count_pending_changes = 0;
  int count_completed_changes = 0;
  Date now;

  if (mNew.size ())
    throw std::string ("Unexpected new tasks found during gc.");

  if (mModified.size ())
    throw std::string ("Unexpected modified tasks found during gc.");

  lock ();

  Filter filter;
  std::vector <Task> ignore;
  loadPending (ignore, filter);

  // Search for dangling dependencies.  These are dependencies whose uuid cannot
  // be converted to an id by TDB.
  std::vector <std::string> deps;
  foreach (task, mPending)
  {
    if (task->has ("depends"))
    {
      deps.clear ();
      task->getDependencies (deps);
      foreach (dep, deps)
      {
        if (id (*dep) == 0)
        {
          task->removeDependency (*dep);
          ++count_pending_changes;
          context.debug ("GC: Removed dangling dependency "
                          + task->get ("uuid")
                          + " -> "
                          + *dep);
        }
      }
    }
  }

  // Now move completed and deleted tasks from the pending list to the
  // completed list.  Isn't garbage collection easy?
  std::vector <Task> still_pending;
  std::vector <Task> newly_completed;
  foreach (task, mPending)
  {
    Task::status s = task->getStatus ();
    if (s == Task::completed ||
        s == Task::deleted)
    {
      newly_completed.push_back (*task);
      ++count_pending_changes;    // removal
      ++count_completed_changes;  // addition
    }
    else if (s == Task::waiting)
    {
      // Wake up tasks that need to be woken.
      Date wait_date (task->get_date ("wait"));
      if (now > wait_date)
      {
        task->setStatus (Task::pending);
        task->remove ("wait");
        ++count_pending_changes;  // modification

        context.debug (std::string ("TDB::gc waiting -> pending for ") +
                       task->get ("uuid"));
      }

      still_pending.push_back (*task);
    }
    else
      still_pending.push_back (*task);
  }

  // No commit - all updates performed manually.
  if (count_pending_changes > 0)
  {
    if (fseek (mLocations[0].pending, 0, SEEK_SET) == 0)
    {
      if (ftruncate (fileno (mLocations[0].pending), 0))
        throw std::string ("Failed to truncate pending.data file ");

      foreach (task, still_pending)
        fputs (task->composeF4 ().c_str (), mLocations[0].pending);

      // Update cached copy.
      mPending = still_pending;
    }
  }

  // Append the new_completed tasks to completed.data.  No need to write out the
  // whole list.
  if (count_completed_changes > 0)
  {
    fseek (mLocations[0].completed, 0, SEEK_END);
    foreach (task, newly_completed)
      fputs (task->composeF4 ().c_str (), mLocations[0].completed);
  }

  // Close files.
  unlock ();

  std::stringstream s;
  s << "gc " << (count_pending_changes + count_completed_changes) << " tasks";
  context.debug (s.str ());
  return count_pending_changes + count_completed_changes;
}

////////////////////////////////////////////////////////////////////////////////
int TDB::nextId ()
{
  return mId++;
}

////////////////////////////////////////////////////////////////////////////////
void TDB::undo ()
{
  Directory location (context.config.get ("data.location"));

  std::string undoFile      = location.data + "/undo.data";
  std::string pendingFile   = location.data + "/pending.data";
  std::string completedFile = location.data + "/completed.data";

  // load undo.data
  std::vector <std::string> u;
  File::read (undoFile, u);

  if (u.size () < 3)
    throw std::string ("There are no recorded transactions to undo.");

  // pop last tx
  u.pop_back (); // separator.

  std::string current = u.back ().substr (4);
  u.pop_back ();

  std::string prior;
  std::string when;
  if (u.back ().substr (0, 5) == "time ")
  {
    when = u.back ().substr (5);
    u.pop_back ();
    prior = "";
  }
  else
  {
    prior = u.back ().substr (4);
    u.pop_back ();
    when = u.back ().substr (5);
    u.pop_back ();
  }

  Date lastChange (atoi (when.c_str ()));

  // Set the colors.
  Color color_red   (context.color () ? context.config.get ("color.undo.before") : "");
  Color color_green (context.color () ? context.config.get ("color.undo.after") : "");

  if (context.config.get ("undo.style") == "side")
  {
    std::cout << "\n"
              << "The last modification was made "
              << lastChange.toString ()
              << "\n";

    // Attributes are all there is, so figure the different attribute names
    // between before and after.
    ViewText view;
    view.width (context.getWidth ());
    view.intraPadding (2);
    view.add (Column::factory ("string", ""));
    view.add (Column::factory ("string", "Prior Values"));
    view.add (Column::factory ("string", "Current Values"));

    Task after (current);

    if (prior != "")
    {
      Task before (prior);

      std::vector <std::string> beforeAtts;
      foreach (att, before)
        beforeAtts.push_back (att->first);

      std::vector <std::string> afterAtts;
      foreach (att, after)
        afterAtts.push_back (att->first);

      std::vector <std::string> beforeOnly;
      std::vector <std::string> afterOnly;
      listDiff (beforeAtts, afterAtts, beforeOnly, afterOnly);

      int row;
      foreach (name, beforeOnly)
      {
        row = view.addRow ();
        view.set (row, 0, *name, red);
        view.set (row, 1, renderAttribute (*name, before.get (*name)), red);
      }

      foreach (name, before)
      {
        std::string priorValue   = before.get (name->first);
        std::string currentValue = after.get  (name->first);

        if (currentValue != "")
        {
          row = view.addRow ();
          view.set (row, 0, name->first);
          view.set (row, 1, renderAttribute (name->first, priorValue), priorValue != currentValue ? color_red : color_green);
          view.set (row, 2, renderAttribute (name->first, currentValue), priorValue != currentValue ? color_red : color_green);
        }
      }

      foreach (name, afterOnly)
      {
        row = view.addRow ();
        view.set (row, 0, *name);
        view.set (row, 2, renderAttribute (*name, after.get (*name)), color_green);
      }
    }
    else
    {
      int row;
      foreach (name, after)
      {
        row = view.addRow ();
        view.set (row, 0, name->first);
        view.set (row, 2, renderAttribute (name->first, after.get (name->first)), color_green);
      }
    }

    std::cout << "\n"
              << view.render ()
              << "\n";
  }

  // This style looks like this:
  //  --- before    2009-07-04 00:00:25.000000000 +0200
  //  +++ after    2009-07-04 00:00:45.000000000 +0200
  //
  // - name: old           // att deleted
  // + name:
  //
  // - name: old           // att changed
  // + name: new
  //
  // - name:
  // + name: new           // att added
  //
  else if (context.config.get ("undo.style") == "diff")
  {
    // Create reference tasks.
    Task before;
    if (prior != "")
      before.parse (prior);

    Task after (current);

    // Generate table header.
    ViewText view
    view.width (context.getWidth ());
    view.intraPadding (2);
    view.addColumn (Column::factory ("string.right", ""));
    view.addColumn (Column::factory ("string", ""));

    int row = view.addRow ();
    view.set (row, 0, "--- previous state", color_red);
    view.set (row, 1, "Undo will restore this state", color_red);

    row = view.addRow ();
    view.set (row, 0, "+++ current state ", color_green);  // Note trailing space.
    view.set (row, 1, "Change made " + lastChange.toString (context.config.get ("dateformat")), color_green);

    view.addRow ();

    // Add rows to table showing diffs.
    std::vector <std::string> all;
    Att::allNames (all);

    // Now factor in the annotation attributes.
    Task::iterator it;
    for (it = before.begin (); it != before.end (); ++it)
      if (it->first.substr (0, 11) == "annotation_")
        all.push_back (it->first);

    for (it = after.begin (); it != after.end (); ++it)
      if (it->first.substr (0, 11) == "annotation_")
        all.push_back (it->first);

    // Now render all the attributes.
    std::sort (all.begin (), all.end ());

    std::string before_att;
    std::string after_att;
    std::string last_att;
    foreach (a, all)
    {
      if (*a != last_att)  // Skip duplicates.
      {
        last_att = *a;

        before_att = before.get (*a);
        after_att  = after.get (*a);

        // Don't report different uuid.
        // Show nothing if values are the unchanged.
        if (*a == "uuid" ||
            before_att == after_att)
        {
          // Show nothing - no point displaying that which did not change.

          // row = view.addRow ();
          // view.set (row, 0, *a + ":");
          // view.set (row, 1, before_att);
        }

        // Attribute deleted.
        else if (before_att != "" && after_att == "")
        {
          row = view.addRow ();
          view.set (row, 0, "-" + *a + ":", color_red);
          view.set (row, 1, before_att, color_red);

          row = view.addRow ();
          view.set (row, 0, "+" + *a + ":", color_green);
        }

        // Attribute added.
        else if (before_att == "" && after_att != "")
        {
          row = view.addRow ();
          view.set (row, 0, "-" + *a + ":", color_red);

          row = view.addRow ();
          view.set (row, 0, "+" + *a + ":", color_green);
          view.set (row, 1, after_att, color_green);
        }

        // Attribute changed.
        else
        {
          row = view.addRow ();
          view.set (row, 0, "-" + *a + ":", color_red);
          view.set (row, 1, before_att, color_red);

          row = view.addRow ();
          view.set (row, 0, "+" + *a + ":", color_green);
          view.set (row, 1, after_att, color_green);
        }
      }
    }

    std::cout << "\n"
              << view.render ()
              << "\n";
  }

  // Output displayed, now confirm.
  if (context.config.getBoolean ("confirmation") &&
      !confirm ("The undo command is not reversible.  Are you sure you want to revert to the previous state?"))
  {
    std::cout << "No changes made.\n";
    return;
  }

  // Extract identifying uuid.
  std::string uuid;
  std::string::size_type uuidAtt = current.find ("uuid:\"");
  if (uuidAtt != std::string::npos)
    uuid = current.substr (uuidAtt, 43); // 43 = uuid:"..."
  else
    throw std::string ("Cannot locate UUID in task to undo.");

  // load pending.data
  std::vector <std::string> p;
  File::read (pendingFile, p);

  // is 'current' in pending?
  foreach (task, p)
  {
    if (task->find (uuid) != std::string::npos)
    {
      context.debug ("TDB::undo - task found in pending.data");

      // Either revert if there was a prior state, or remove the task.
      if (prior != "")
      {
        *task = prior;
        std::cout << "Modified task reverted.\n";
      }
      else
      {
        p.erase (task);
        std::cout << "Task removed.\n";
      }

      // Rewrite files.
      File::write (pendingFile, p);
      File::write (undoFile, u);
      return;
    }
  }

  // load completed.data
  std::vector <std::string> c;
  File::read (completedFile, c);

  // is 'current' in completed?
  foreach (task, c)
  {
    if (task->find (uuid) != std::string::npos)
    {
      context.debug ("TDB::undo - task found in completed.data");

      // If task now belongs back in pending.data
      if (prior.find ("status:\"pending\"")   != std::string::npos ||
          prior.find ("status:\"waiting\"")   != std::string::npos ||
          prior.find ("status:\"recurring\"") != std::string::npos)
      {
        c.erase (task);
        p.push_back (prior);
        File::write (completedFile, c);
        File::write (pendingFile, p);
        File::write (undoFile, u);
        std::cout << "Modified task reverted.\n";
        context.debug ("TDB::undo - task belongs in pending.data");
      }
      else
      {
        *task = prior;
        File::write (completedFile, c);
        File::write (undoFile, u);
        std::cout << "Modified task reverted.\n";
        context.debug ("TDB::undo - task belongs in completed.data");
      }

      std::cout << "Undo complete.\n";
      return;
    }
  }

  // Perhaps user hand-edited the data files?
  // Perhaps the task was in completed.data, which was still in file format 3?
  std::cout << "Task with UUID "
            << uuid.substr (6, 36)
            << " not found in data.\n"
            << "No undo possible.\n";
}

////////////////////////////////////////////////////////////////////////////////
void TDB::merge (const std::string& mergeFile)
{
  ///////////////////////////////////////
  // Copyright 2010 - 2011, Johannes Schlatow.
  ///////////////////////////////////////

  // list of modifications that we want to add to the local database
  std::list<Taskmod> mods;

  // list of modifications on the local database
  // has to be merged with mods to create the new undo.data
  std::list<Taskmod> lmods;

  // will contain the NEW undo.data
  std::vector <std::string> undo;

  ///////////////////////////////////////
  // initialize the files:

  // load merge file (undo file of right/remote branch)
  std::vector <std::string> r;
  if (! File::read (mergeFile, r))
    throw std::string ("Could not read '") + mergeFile + "'.";

  // file has to contain at least one entry
  if (r.size () < 3)
    throw std::string ("There are no changes to merge.");

  // load undo file (left/local branch)
  Directory location (context.config.get ("data.location"));
  std::string undoFile = location.data + "/undo.data";

  std::vector <std::string> l;
  if (! File::read (undoFile, l))
    throw std::string ("Could not read '") + undoFile + "'.";

  std::string rline, lline;
  std::vector <std::string>::iterator rit, lit;

  // read first line
  rit = r.begin ();
  lit = l.begin ();

  if (rit != r.end())
    rline = *rit;
  if (lit != l.end())
    lline = *lit;

  ///////////////////////////////////////
  // find the branch-off point:

  // first lines are not equal => assuming mergeFile starts at a
  // later point in time
  if (lline.compare (rline) != 0)
  {
    // iterate in local file to find rline
    for ( ; lit != l.end (); ++lit)
    {
      lline = *lit;

      // push the line to the new undo.data
      undo.push_back (lline + "\n");

      // found first matching lines?
      if (lline.compare (rline) == 0)
        break;
    }
  }

  // Add some color.
  Color colorAdded    (context.config.get ("color.sync.added"));
  Color colorChanged  (context.config.get ("color.sync.changed"));
  Color colorRejected (context.config.get ("color.sync.rejected"));

  // at this point we can assume: (lline==rline) || (lit == l.end())
  // thus we search for the first non-equal lines or the EOF
  bool found = false;
  for ( ; (lit != l.end ()) && (rit != r.end ()); ++lit, ++rit)
  {
    lline = *lit;
    rline = *rit;

    // found first non-matching lines?
    if (lline.compare (rline) != 0)
    {
      found = true;
      break;
    }
    else
    {
      // push the line to the new undo.data
      undo.push_back (lline + "\n");
    }
  }

  std::cout << "\n";

  ///////////////////////////////////////
  // branch-off point found:
  if (found)
  {
    DEBUG_STR_PART ("Branch-off point found at: ");
    DEBUG_STR_END (lline);

    std::list<Taskmod> rmods;

    // helper lists
    std::set<std::string> uuid_new, uuid_left;

    // 1. read taskmods out of the remaining lines
    readTaskmods (l, lit, lmods);
    readTaskmods (r, rit, rmods);

    // 2. move new uuids into mods
    DEBUG_STR_PART ("adding new uuids (left) to skip list...");

    // modifications on the left side are already in the database
    // we just need them to merge conflicts, so we add new the mods for
    // new uuids to the skip-list 'uuid_left'
    std::list<Taskmod>::iterator lmod_it;
    for (lmod_it = lmods.begin (); lmod_it != lmods.end (); lmod_it++)
    {
      if (lmod_it->isNew ())
      {
/*
        std::cout << "New local task                     "
                  << (context.color () ? colorAdded.colorize (lmod_it->getUuid ()) : lmod_it->getUuid ())
                  << "\n";
*/

        uuid_left.insert (lmod_it->getUuid ());
      }
    }

    DEBUG_STR_END ("done");
    DEBUG_STR_PART ("move new uuids (right) to redo list...");

    // new items on the right side need to be inserted into the
    // local database
    std::list<Taskmod>::iterator rmod_it;
    for (rmod_it = rmods.begin (); rmod_it != rmods.end (); )
    {
      // we have to save and increment the iterator because we may want to delete
      // the object from the list
      std::list<Taskmod>::iterator current = rmod_it++;
      Taskmod tmod = *current;

      // new uuid?
      if (tmod.isNew ())
      {
/*
        std::cout << "Adding new remote task             "
                  << (context.color () ? colorAdded.colorize (tmod.getUuid ()) : tmod.getUuid ())
                  << "\n";
*/

        uuid_new.insert (tmod.getUuid ());
        mods.push_back (tmod);
        rmods.erase (current);
      }
      else if (uuid_new.find (tmod.getUuid ()) != uuid_new.end ())
      {
        // uuid of modification was new
        mods.push_back (tmod);
        rmods.erase (current);
      }
    }

    DEBUG_STR_END ("done");

    ///////////////////////////////////////
    // merge modifications:
    DEBUG_STR ("Merging modifications:");

    // we iterate backwards to resolve conflicts by timestamps (newest one wins)
    std::list<Taskmod>::reverse_iterator lmod_rit;
    std::list<Taskmod>::reverse_iterator rmod_rit;
    for (lmod_rit = lmods.rbegin (); lmod_rit != lmods.rend (); ++lmod_rit)
    {
      Taskmod     tmod_l = *lmod_rit;
      std::string uuid   = tmod_l.getUuid ();

      DEBUG_STR ("  left uuid: " + uuid);

      // skip if uuid had already been merged
      if (uuid_left.find (uuid) == uuid_left.end ())
      {
        bool rwin = false;
        bool lwin = false;
        for (rmod_rit = rmods.rbegin (); rmod_rit != rmods.rend (); rmod_rit++)
        {
          Taskmod tmod_r = *rmod_rit;

          DEBUG_STR ("    right uuid: " + tmod_r.getUuid ());
          if (tmod_r.getUuid () == uuid)
          {
            DEBUG_STR ("    uuid match found for " + uuid);

            // we already decided to take the mods from the right side
            // but we have to find the first modification newer than
            // the one on the left side to merge the history too
            if (rwin)
            {
              DEBUG_STR ("    scanning right side");
              if (tmod_r > tmod_l)
                mods.push_front (tmod_r);

              std::list<Taskmod>::iterator tmp_it = rmod_rit.base ();
              rmods.erase (--tmp_it);
              rmod_rit--;
            }
            else if (lwin)
            {
              DEBUG_STR ("    cleaning up right side");

              std::list<Taskmod>::iterator tmp_it = rmod_rit.base ();
              rmods.erase (--tmp_it);
              rmod_rit--;
            }
            else
            {
              // which one is newer?
              if (tmod_r > tmod_l)
              {
                std::cout << "Found remote change to        "
                          << (context.color () ? colorChanged.colorize (uuid) : uuid)
                          << "  \"" << cutOff (tmod_r.getBefore ().get ("description"), 10) << "\""
                          << "\n";

                mods.push_front(tmod_r);

                // delete tmod from right side
                std::list<Taskmod>::iterator tmp_it = rmod_rit.base ();
                rmods.erase (--tmp_it);
                rmod_rit--;

                rwin = true;
              }
              else
              {
                std::cout << "Retaining local changes to    "
                          << (context.color () ? colorRejected.colorize (uuid) : uuid)
                          << "  \"" << cutOff (tmod_l.getBefore ().get ("description"), 10) << "\""
                          << "\n";

                // inserting right mod into history of local database
                // so that it can be restored later

                // TODO feature: make rejected changes on the remote branch restorable
//                Taskmod reverse_tmod;
//
//                tmod_r.setBefore(lmod_rit->getAfter());
//                tmod_r.setTimestamp(lmod_rit->getTimestamp()+1);
//
//                reverse_tmod.setAfter(tmod_r.getBefore());
//                reverse_tmod.setBefore(tmod_r.getAfter());
//                reverse_tmod.setTimestamp(tmod_r.getTimestamp());
//
//                mods.push_back(tmod_r);
//                mods.push_back(reverse_tmod);

                // delete tmod from right side
                std::list<Taskmod>::iterator tmp_it = rmod_rit.base ();
                rmods.erase (--tmp_it);
                rmod_rit--;

                // mark this uuid as merged
                uuid_left.insert (uuid);
                lwin = true;
              }
            }
          }
        } // for

        if (rwin)
        {
          DEBUG_STR ("  concat the first match to left branch");
          // concat the oldest (but still newer) modification on the right
          // to the endpoint on the left
          mods.front ().setBefore(tmod_l.getAfter ());
        }
      }
    } // for

    DEBUG_STR ("adding non-conflicting changes from the right branch");
    mods.splice (mods.begin (), rmods);

    DEBUG_STR ("sorting taskmod list");
    mods.sort ();
  }
  else if (rit == r.end ())
  {
    // nothing happend on the remote branch
    // local branch is up-to-date

    // nothing happend on the local branch either

    // break, to suppress autopush
    if (lit == l.end ())
    {
      mods.clear ();
      lmods.clear ();
      throw std::string ("Database is up-to-date, no merge required.");
    }

  }
  else // lit == l.end ()
  {
    // nothing happend on the local branch
/*
    std::cout << "No local changes detected.\n";
*/

    // add remaining lines (remote branch) to the list of modifications
/*
    std::cout << "Remote changes detected.\n";
*/
    readTaskmods (r, rit, mods);
  }

  ///////////////////////////////////////
  // Now apply the changes.
  // redo command:

  if (!mods.empty ())
  {
    std::string pendingFile   = location.data + "/pending.data";
    std::vector <std::string> pending;

    std::string completedFile = location.data + "/completed.data";
    std::vector <std::string> completed;

    if (! File::read (pendingFile, pending))
      throw std::string ("Could not read '") + pendingFile + "'.";

    if (! File::read (completedFile, completed))
      throw std::string ("Could not read '") + completedFile + "'.";

    // iterate over taskmod list
    std::list<Taskmod>::iterator it;
    for (it = mods.begin (); it != mods.end (); )
    {
      std::list<Taskmod>::iterator current = it++;
      Taskmod tmod = *current;

      // Modification to an existing task.
      if (!tmod.isNew ())
      {
        std::string uuid = tmod.getUuid ();
        Task::status statusBefore = tmod.getBefore().getStatus ();
        Task::status statusAfter  = tmod.getAfter().getStatus ();

        std::vector <std::string>::iterator it;

        bool found = false;
        if ( (statusBefore == Task::completed)
          || (statusBefore == Task::deleted) )
        {
          // Find the same uuid in completed data
          for (it = completed.begin (); it != completed.end (); ++it)
          {
            if (it->find ("uuid:\"" + uuid) != std::string::npos)
            {
              // Update the completed record.
/*
              std::cout << "Modifying                     "
                        << (context.color () ? colorChanged.colorize (uuid) : uuid)
                        << "\n";
*/

              // remove the \n from composeF4() string
              std::string newline = tmod.getAfter ().composeF4 ();
              newline = newline.substr (0, newline.length ()-1);

              // does the tasks move to pending data?
              // this taskmod will not arise from
              // normal usage of task, but those kinds of
              // taskmods may be constructed to merge databases
              if ( (statusAfter != Task::completed)
                && (statusAfter != Task::deleted) )
              {
                // insert task into pending data
                pending.push_back (newline);

                // remove task from completed data
                completed.erase (it);

              }
              else
              {
                // replace the current line
                *it = newline;
              }

              found = true;
              break;
            }
          }
        }
        else
        {
          // Find the same uuid in the pending data.
          for (it = pending.begin (); it != pending.end (); ++it)
          {
            if (it->find ("uuid:\"" + uuid) != std::string::npos)
            {
              // Update the pending record.
              std::cout << "Found remote change to        "
                        << (context.color () ? colorChanged.colorize (uuid) : uuid)
                        << "  \"" << cutOff (tmod.getBefore ().get ("description"), 10) << "\""
                        << "\n";

              // remove the \n from composeF4() string
              // which will replace the current line
              std::string newline = tmod.getAfter ().composeF4 ();
              newline = newline.substr (0, newline.length ()-1);

              // does the tasks move to completed data
              if ( (statusAfter == Task::completed)
                || (statusAfter == Task::deleted) )
              {
                // insert task into completed data
                completed.push_back (newline);

                // remove task from pending data
                pending.erase (it);

              }
              else
              {
                // replace the current line
                *it = newline;
              }

              found = true;
              break;
            }
          }
        }

        if (!found)
        {
          std::cout << "Missing                       "
                    << (context.color () ? colorRejected.colorize (uuid) : uuid)
                    << "  \"" << cutOff (tmod.getBefore ().get ("description"), 10) << "\""
                    << "\n";
          mods.erase (current);
        }
      }
      else
      {
        // Check for dups.
        std::string uuid = tmod.getAfter ().get ("uuid");

        // Find the same uuid in the pending data.
        bool found = false;
        std::vector <std::string>::iterator pit;
        for (pit = pending.begin (); pit != pending.end (); ++pit)
        {
          if (pit->find ("uuid:\"" + uuid) != std::string::npos)
          {
            found = true;
            break;
          }
        }

        if (!found)
        {
          std::cout << "Merging new remote task       "
                    << (context.color () ? colorAdded.colorize (uuid) : uuid)
                    << "  \"" << cutOff (tmod.getAfter ().get ("description"), 10) << "\""
                    << "\n";

          // remove the \n from composeF4() string
          std::string newline = tmod.getAfter ().composeF4 ();
          newline = newline.substr (0, newline.length ()-1);
          pending.push_back (newline);
        }
        else
        {
          mods.erase (current);
        }
      }
    }

    // write pending file
    if (! File::write (pendingFile, pending))
      throw std::string ("Could not write '") + pendingFile + "'.";

    // write completed file
    if (! File::write (completedFile, completed))
      throw std::string ("Could not write '") + completedFile + "'.";

    // at this point undo contains the lines up to the branch-off point
    // now we merge mods (new modifications from mergefile)
    // with lmods (part of old undo.data)
    mods.merge (lmods);

    // generate undo.data format
    for (it = mods.begin (); it != mods.end (); it++)
      undo.push_back(it->toString ());

    // write undo file
    if (! File::write (undoFile, undo, false))
      throw std::string ("Could not write '") + undoFile + "'.";
  }

  // delete objects
  lmods.clear ();
  mods.clear ();
}

////////////////////////////////////////////////////////////////////////////////
std::string TDB::uuid (int id) const
{
  std::map <int, std::string>::const_iterator i;
  if ((i = mI2U.find (id)) != mI2U.end ())
    return i->second;

  return "";
}

////////////////////////////////////////////////////////////////////////////////
int TDB::id (const std::string& uuid) const
{
  std::map <std::string, int>::const_iterator i;
  if ((i = mU2I.find (uuid)) != mU2I.end ())
    return i->second;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
FILE* TDB::openAndLock (const std::string& file)
{
  // TODO Need provision here for read-only locations.

  // Check for access.
  File f (file);
  bool exists = f.exists ();
  if (exists)
    if (!f.readable () || !f.writable ())
      throw std::string ("Task does not have the correct permissions for '") +
            file + "'.";

  // Open the file.
  FILE* in = fopen (file.c_str (), (exists ? "r+" : "w+"));
  if (!in)
    throw std::string ("Could not open '") + file + "'.";

  // Lock if desired.  Try three times before failing.
  if (mLock)
  {
    int retry = 0;
    while (flock (fileno (in), LOCK_NB | LOCK_EX) && ++retry <= 3)
    {
      std::cout << "Waiting for file lock...\n";
      while (flock (fileno (in), LOCK_NB | LOCK_EX) && ++retry <= 3)
        delay (0.2);
    }

    if (retry > 3)
      throw std::string ("Could not lock '") + file + "'.";
  }

  return in;
}

////////////////////////////////////////////////////////////////////////////////
void TDB::writeUndo (const Task& after, FILE* file)
{
  fprintf (file,
           "time %u\nnew %s---\n",
           (unsigned int) time (NULL),
           after.composeF4 ().c_str ());
}

////////////////////////////////////////////////////////////////////////////////
void TDB::writeUndo (const Task& before, const Task& after, FILE* file)
{
  fprintf (file,
           "time %u\nold %snew %s---\n",
           (unsigned int) time (NULL),
           before.composeF4 ().c_str (),
           after.composeF4 ().c_str ());
}

////////////////////////////////////////////////////////////////////////////////
bool TDB::uuidAlreadyUsed (
  const std::string& uuid,
  const std::vector <Task>& all)
{
  std::vector <Task>::const_iterator it;
  for (it = all.begin (); it != all.end (); ++it)
    if (it->get ("uuid") == uuid)
      return true;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
#endif

