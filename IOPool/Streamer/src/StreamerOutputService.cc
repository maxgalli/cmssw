// $Id: StreamerOutputService.cc,v 1.19 2007/01/22 22:04:53 wmtan Exp $

#include "IOPool/Streamer/interface/EventStreamOutput.h"
#include "IOPool/Streamer/interface/StreamerOutputService.h"
#include "IOPool/Streamer/interface/InitMsgBuilder.h"
#include "IOPool/Streamer/interface/EventMsgBuilder.h"
#include "IOPool/Streamer/interface/EOFRecordBuilder.h"
#include "IOPool/Streamer/interface/MsgTools.h"
#include "IOPool/Streamer/interface/DumpTools.h"

// to stat files and directories
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

namespace edm
{

/* No one will use this CTOR anyways, we can remove it in future */
StreamerOutputService::StreamerOutputService():
  maxFileSize_(1024*1024*1024),
  maxFileEventCount_(50),
  currentFileSize_(0),
  totalEventCount_(0),
  eventsInFile_(0),
  fileNameCounter_(0),
  files_(),
  filen_(),
  highWaterMark_(0.9),
  path_(),
  mpath_(),
  diskUsage_(0.0),
  closedFiles_(),
  catalog_(),
  nLogicalDisk_(0),
  saved_initmsg_(),
  fileName_(),
  lockFileName_(),
  streamNindex_writer_(),
  requestParamSet_(),
  eventSelector_(),
  statistics_()
  {
    saved_initmsg_[0] = '\0';
  }

 StreamerOutputService::StreamerOutputService(edm::ParameterSet const& ps):
 //maxFileSize_(ps.template getParameter<int>("maxFileSize")),
 //maxFileEventCount_(ps.template getParameter<int>("maxFileEventCount")),
 // defaulting - need numbers from Emilio
 //StreamerOutputService::StreamerOutputService():
  maxFileSize_(1024*1024*1024),
  maxFileEventCount_(50),
  currentFileSize_(0),
  totalEventCount_(0),
  eventsInFile_(0),
  fileNameCounter_(0),
  files_(),
  filen_(),
  highWaterMark_(0.9),
  path_(),
  mpath_(),
  diskUsage_(0.0),
  closedFiles_(),
  catalog_(),
  nLogicalDisk_(0),
  saved_initmsg_(),
  fileName_(),
  lockFileName_(),
  streamNindex_writer_(),
  requestParamSet_(ps),
  eventSelector_(),
  statistics_()
  {
    saved_initmsg_[0] = '\0';
  }

void StreamerOutputService::init(std::string fileName, unsigned int maxFileSize, double highWaterMark,
                                 std::string path, std::string mpath,
				 std::string catalog, uint32 disks,
				 InitMsgView const& view)
  {
   maxFileSize_ = maxFileSize;
   highWaterMark_ = highWaterMark;
   path_ = path;
   mpath_ = mpath;
   filen_ = fileName;
   nLogicalDisk_ = disks;

   // create file names (can be move to seperate method)
   std::ostringstream newFileName;
   newFileName << path_ << "/";
   catalog_      = newFileName.str() + catalog;
   lockFileName_ = newFileName.str() + "nolock";

   if (nLogicalDisk_ != 0)
     {
       newFileName << (fileNameCounter_ % nLogicalDisk_) << "/";
       lockFileName_ = newFileName.str() + ".lock";
       ofstream *lockFile = new ofstream(lockFileName_.c_str(), ios_base::ate | ios_base::out | ios_base::app);
       delete(lockFile);
     }

   newFileName << filen_ << "." << fileNameCounter_ ;
   fileName_      = newFileName.str() + ".dat";
   indexFileName_ = newFileName.str() + ".ind";

   statistics_ = boost::shared_ptr<edm::StreamerStatWriteService>
     (new edm::StreamerStatWriteService(0, "-", indexFileName_, fileName_, catalog_));

   streamNindex_writer_ = boost::shared_ptr<StreamerFileWriter>(new StreamerFileWriter(fileName_, indexFileName_));


   //dumpInitHeader(&view);

   writeHeader(view);

   //INIT msg can be saved as INIT msg itself.

   // save the INIT message for when writing to the next file
   // that is openned
   char* pos = &saved_initmsg_[0];
   unsigned char* from = view.startAddress();
   unsigned int dsize = view.size();
   copy(from, from + dsize, pos);

   // initialize event selector
   initializeSelection(view);

  }

void StreamerOutputService::initializeSelection(InitMsgView const& initView)
  {
  Strings triggerNameList;
  initView.hltTriggerNames(triggerNameList);

  // fake the process name (not yet available from the init message?)
  std::string processName = "HLT";

  /* ---printout the trigger names in the INIT message*/
  std::cout << ">>>>>>>>>>>Trigger names:" << std::endl;
  for (Strings::const_iterator it = triggerNameList.begin(), itEnd = triggerNameList.end(); it != itEnd; ++it)
    std::cout << ">>>>>>>>>>>  name = " << *it << std::endl;
  /* */

  // create our event selector
    eventSelector_.reset(
      new EventSelector(requestParamSet_.getUntrackedParameter("SelectEvents", ParameterSet()),
                        triggerNameList));
  }

StreamerOutputService::~StreamerOutputService()
  {
    // write to summary catalog and mailbox if file has an entry.
    if (eventsInFile_ > 0) {
	writeToMailBox();
	statistics_->setFileSize(static_cast<uint32>(currentFileSize_));
	statistics_->setEventCount(static_cast<uint32>(eventsInFile_));
	statistics_->writeStat();
    }

    std::ostringstream newFileName;
    newFileName << path_ << "/";
    if (nLogicalDisk_ != 0) {
	newFileName << (fileNameCounter_ % nLogicalDisk_) << "/";
	remove(lockFileName_.c_str());
    }

    std::ostringstream entry;
    entry << fileNameCounter_ << " "
	  << fileName_
	  << " " << eventsInFile_
	  << "   " << currentFileSize_;
    files_.push_back(entry.str());
    closedFiles_ += ", ";
    closedFiles_ += fileName_;

    std::cout << "#    name                             evt        size     " << endl;
    for(std::list<std::string>::const_iterator it = files_.begin(), itEnd = files_.end();
	  it != itEnd; ++it) {
      std::cout << *it << endl;
    }
    std::cout << "Disk Usage = " << diskUsage_ << endl;
    std::cout << "Closed files = " << closedFiles_ << endl;
  }

void StreamerOutputService::stop()
  {
    //Does EOF record writting and HLT Event count for each path for EOF
    streamNindex_writer_->stop();
    // gives the EOF Size
    currentFileSize_ += streamNindex_writer_->getStreamEOFSize();
  }

void StreamerOutputService::writeHeader(InitMsgView const& init_message)
  {
    //Write the Init Message to Streamer and Index file
    streamNindex_writer_->doOutputHeader(init_message);

    currentFileSize_ += init_message.size();

  }

bool StreamerOutputService::writeEvent(EventMsgView const& eview, uint32 hltsize)
  {
    bool returnVal = true;

    //Check if this event meets the selection criteria, if not skip it.
    if (!wantsEvent(eview))
      return returnVal;

    // since only the file size is checked here, we don't care that one event
    // will be written even though the condition for closing the file
    // is satisfied ... has to change later.
    if (currentFileSize_ >= maxFileSize_)
      returnVal = false;

    //Write the Event Message to Streamer and index
    streamNindex_writer_->doOutputEvent(eview);

    ++eventsInFile_;
    ++totalEventCount_;
    currentFileSize_ += eview.size();

    return returnVal;
  }

void StreamerOutputService::closeFile(EventMsgView const& eview)
  {
    checkFileSystem(); // later should take some action
    stop();
    writeToMailBox();

    statistics_->setFileSize(static_cast<uint32>(currentFileSize_));
    statistics_->setEventCount(static_cast<uint32>(eventsInFile_));
    statistics_->setRunNumber(static_cast<uint32>(eview.run()));
    statistics_->writeStat();

    ++fileNameCounter_;

    string tobeclosedFile = fileName_;

    std::ostringstream newFileName;
    newFileName << path_ << "/";
    if (nLogicalDisk_ != 0) {
	newFileName << (fileNameCounter_ % nLogicalDisk_) << "/";
	remove(lockFileName_.c_str());
	lockFileName_ = newFileName.str() + ".lock";
	ofstream *lockFile =
	  new ofstream(lockFileName_.c_str(), ios_base::ate | ios_base::out | ios_base::app);
	delete(lockFile);
    }

    newFileName << filen_ << "." << fileNameCounter_ ;
    fileName_      = newFileName.str() + ".dat";
    indexFileName_ = newFileName.str() + ".ind";

    statistics_ = boost::shared_ptr<edm::StreamerStatWriteService>
      (new edm::StreamerStatWriteService(eview.run(), "-", indexFileName_, fileName_, catalog_));

    streamNindex_writer_.reset(new StreamerFileWriter(fileName_, indexFileName_));

    // write out the summary line for this last file
    std::ostringstream entry;
    entry << (fileNameCounter_ - 1) << " "
	  << tobeclosedFile
	  << " " << eventsInFile_
	  << "   " << currentFileSize_;
    files_.push_back(entry.str());
    if(fileNameCounter_!=1) closedFiles_ += ", ";
    closedFiles_ += tobeclosedFile;

    eventsInFile_ = 0;
    currentFileSize_ = 0;
    // write the Header for the newly openned file
    // from the previously saved INIT msg
    InitMsgView myview(&saved_initmsg_[0]);

    writeHeader(myview);
  }

bool StreamerOutputService::wantsEvent(EventMsgView const& eventView)
  {
    std::vector<unsigned char> hlt_out;
    hlt_out.resize(1 + (eventView.hltCount()-1)/4);
    eventView.hltTriggerBits(&hlt_out[0]);
    /* --- print the trigger bits from the event header
    std::cout << ">>>>>>>>>>>Trigger bits:" << std::endl;
    for(int i = 0; i < hlt_out.size(); ++i)
    {
      unsigned int test = static_cast<unsigned int>(hlt_out[i]);
      std::cout<< hex << ">>>>>>>>>>>  bits = " << test << " " << hlt_out[i] << std::endl;
    }
    cout << "\nhlt bits=\n(";
    for(int i=(hlt_out.size()-1); i != -1 ; --i)
       printBits(hlt_out[i]);
    cout << ")\n";
    */
    int num_paths = eventView.hltCount();
    //cout <<"num_paths: "<<num_paths<<endl;
    bool rc = (eventSelector_->wantAll() || eventSelector_->acceptEvent(&hlt_out[0], num_paths));
    //std::cout << "====================== " << std::endl;
    //std::cout << "return selector code = " << rc << std::endl;
    //std::cout << "====================== " << std::endl;
    return rc;
  }

  void StreamerOutputService::writeToMailBox()
  {
    std::ostringstream ofilename;
    ofilename << mpath_ << "/" << filen_ << "." << fileNameCounter_ << ".smry";
    ofstream of(ofilename.str().c_str());
    of << fileName_;
    of.close();
  }

  void StreamerOutputService::checkFileSystem()
  {
    struct statfs64 buf;
    int retVal = statfs64(path_.c_str(), &buf);
    if(retVal!=0)
      //edm::LogWarning("StreamerOutputService") << "Could not stat output filesystem for path "
      std::cout << "StreamerOutputService: " << "Could not stat output filesystem for path "
                                               << path_ << std::endl;

    unsigned int btotal = 0;
    unsigned int bfree = 0;
    unsigned int blksize = 0;
    if(retVal==0)
      {
        blksize = buf.f_bsize;
        btotal = buf.f_blocks;
        bfree = buf.f_bfree;
      }
    float dfree = float(bfree)/float(btotal);
    float dusage = 1. - dfree;
    diskUsage_ = dusage;
    if(dusage>highWaterMark_)
      //edm::LogWarning("StreamerOutputService") << "Output filesystem for path " << path_
      std::cout << "StreamerOutputService: " << "Output filesystem for path " << path_
                                 << " is more than " << highWaterMark_*100 << "% full " << std::endl;
  }

}  //end-of-namespace block
