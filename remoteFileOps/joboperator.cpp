/*********************************************************************************
**
** Copyright (c) 2017 The University of Notre Dame
** Copyright (c) 2017 The Regents of the University of California
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice, this
** list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice, this
** list of conditions and the following disclaimer in the documentation and/or other
** materials provided with the distribution.
**
** 3. Neither the name of the copyright holder nor the names of its contributors may
** be used to endorse or promote products derived from this software without specific
** prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
** EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
** SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
** IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
***********************************************************************************/

// Contributors:
// Written by Peter Sempolinski, for the Natural Hazard Modeling Laboratory, director: Ahsan Kareem, at Notre Dame

#include "joboperator.h"

#include "../ae_globals.h"

#include "../AgaveExplorer/remoteModelViews/remotejoblister.h"
#include "../AgaveClientInterface/remotedatainterface.h"
#include "../AgaveClientInterface/remotejobdata.h"
#include "joblistnode.h"

JobOperator::JobOperator(QObject * parent) : QObject((QObject *)parent)
{
    theJobList.setHorizontalHeaderLabels({"Task Name", "State", "Agave App", "Time Created", "Agave ID"});
}

JobOperator::~JobOperator()
{
    for (auto itr = jobData.begin(); itr != jobData.end(); itr++)
    {
        delete (*itr);
    }
}

void JobOperator::linkToJobLister(RemoteJobLister * newLister)
{
    newLister->setModel(&theJobList);
}

void JobOperator::refreshRunningJobList(RequestState replyState, QList<RemoteJobData> theData)
{
    //Note: RemoteDataReply destroys itself after signal
    currentJobReply = NULL;
    if (replyState != RequestState::GOOD)
    {
        ae_globals::displayPopup("Error: unable to list jobs. Please check your network connection and try again.", "Network Error:");
        QTimer::singleShot(5000, this, SLOT(demandJobDataRefresh()));
        return;
    }

    bool notDone = false;

    for (auto itr = theData.rbegin(); itr != theData.rend(); itr++)
    {
        if (jobData.contains((*itr).getID()))
        {
            JobListNode * theItem = jobData.value((*itr).getID());
            theItem->setData(*itr);
        }
        else
        {
            JobListNode * theItem = new JobListNode(*itr, &theJobList);
            jobData.insert(theItem->getData()->getID(), theItem);
        }
        if (!notDone && ((*itr).getState() != "FINISHED") && ((*itr).getState() != "FAILED"))
        {
            notDone = true;
        }
    }

    emit newJobData();

    if (notDone)
    {
        QTimer::singleShot(5000, this, SLOT(demandJobDataRefresh()));
    }
}

QMap<QString, const RemoteJobData *> JobOperator::getJobsList()
{
    QMap<QString, const RemoteJobData *> ret;

    for (auto itr = jobData.cbegin(); itr != jobData.cend(); itr++)
    {
        ret.insert((*itr)->getData()->getID(), (*itr)->getData());
    }

    return ret;
}

void JobOperator::requestJobDetails(const RemoteJobData *toFetch)
{
    if (!jobData.contains(toFetch->getID()))
    {
        return;
    }
    JobListNode * realNode = jobData.value(toFetch->getID());

    if (realNode->haveDetails()) return;
    if (realNode->haveDetailTask()) return;

    RemoteDataReply * jobReply = ae_globals::get_connection()->getJobDetails(toFetch->getID());

    if (jobReply == NULL) return; //TODO: Consider an error message here

    realNode->setDetailTask(jobReply);
}

void JobOperator::underlyingJobChanged()
{
    emit newJobData();
}

const RemoteJobData * JobOperator::findJobByID(QString idToFind)
{
    if (!jobData.contains(idToFind))
    {
        return NULL;
    }

    return jobData.value(idToFind)->getData();
}

void JobOperator::demandJobDataRefresh()
{
    if (currentJobReply != NULL)
    {
        return;
    }
    currentJobReply = ae_globals::get_connection()->getListOfJobs();
    QObject::connect(currentJobReply, SIGNAL(haveJobList(RequestState,QList<RemoteJobData>)),
                     this, SLOT(refreshRunningJobList(RequestState,QList<RemoteJobData>)));
}
