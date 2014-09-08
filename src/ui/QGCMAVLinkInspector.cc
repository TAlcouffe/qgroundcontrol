#include <QList>

#include "QGCMAVLink.h"
#include "QGCMAVLinkInspector.h"
#include "UASManager.h"
#include "ui_QGCMAVLinkInspector.h"

#include <QDebug>

const float QGCMAVLinkInspector::updateHzLowpass = 0.2f;
const unsigned int QGCMAVLinkInspector::updateInterval = 1000U;

QGCMAVLinkInspector::QGCMAVLinkInspector(MAVLinkProtocol* protocol, QWidget *parent) :
    QWidget(parent),
    selectedSystemID(0),
    selectedComponentID(0),
    ui(new Ui::QGCMAVLinkInspector)
{
	qDebug() << "Begining init MAVLinkInspector";

    ui->setupUi(this);

	uasCreated = false;

    // Make sure "All" is an option for both the system and components
    ui->systemComboBox->addItem(tr("All"), 0);
    ui->componentComboBox->addItem(tr("All"), 0);

	// Store metadata for all MAVLink messages.
	mavlink_message_info_t msg_infos[256] = MAVLINK_MESSAGE_INFO;
	memcpy(messageInfo, msg_infos, sizeof(mavlink_message_info_t)*256);

	// Initialize the received data for all messages to invalid (0xFF)
    //memset(receivedMessages, 0xFF, sizeof(mavlink_message_t)*256);

	mavlink_protocol = protocol;

	// Set up the column headers for the message listing
    QStringList header;
    header << tr("Name");
    header << tr("Value");
    header << tr("Type");
	header << tr("Frequency");
    ui->treeWidget->setHeaderLabels(header);

    // Connect the UI
    connect(ui->systemComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectDropDownMenuSystem(int)));
    connect(ui->componentComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectDropDownMenuComponent(int)));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clearView()));

    // Connect external connections
    connect(UASManager::instance(), SIGNAL(UASCreated(UASInterface*)), this, SLOT(addSystem(UASInterface*)));
    connect(protocol, SIGNAL(messageReceived(LinkInterface*,mavlink_message_t)), this, SLOT(receiveMessage(LinkInterface*,mavlink_message_t)));

    // Attach the UI's refresh rate to a timer.
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(refreshView()));
    updateTimer.start(updateInterval);

	// connect to activate the streams
	connect(ui->treeWidget,SIGNAL(itemClicked(QTreeWidgetItem*,int)),this,SLOT(activateStream(QTreeWidgetItem*,int)));
	//connect(ui->treeWidget,SIGNAL(itemChanged(QTreeWidgetItem*,int)),this,SLOT(activateStream(QTreeWidgetItem*,int)));

	connect(ui->sendStreamButton,SIGNAL(clicked()),this,SLOT(sendStreamButton()));

	qDebug() << "End init MAVLinkInspector";
}

void QGCMAVLinkInspector::addSystem(UASInterface* uas)
{
	qDebug() << "UAS created";
	uasCreated = true;
    ui->systemComboBox->addItem(uas->getUASName(), uas->getUASID());
}

void QGCMAVLinkInspector::selectDropDownMenuSystem(int dropdownid)
{
    selectedSystemID = ui->systemComboBox->itemData(dropdownid).toInt();
    rebuildComponentList();
}

void QGCMAVLinkInspector::selectDropDownMenuComponent(int dropdownid)
{
    selectedComponentID = ui->componentComboBox->itemData(dropdownid).toInt();
}

void QGCMAVLinkInspector::rebuildComponentList()
{
    ui->componentComboBox->clear();

    // Fill
    UASInterface* uas = UASManager::instance()->getUASForId(selectedSystemID);
    if (uas)
    {
        QMap<int, QString> components = uas->getComponents();

        foreach (int id, components.keys())
        {
            QString name = components.value(id);
            ui->componentComboBox->addItem(name, id);
        }
    }
}

void QGCMAVLinkInspector::addComponent(int uas, int component, const QString& name)
{
    Q_UNUSED(component);
    Q_UNUSED(name);
    if (uas != selectedSystemID) return;

    rebuildComponentList();
}

/**
 * Reset the view. This entails clearing all data structures and resetting data from already-
 * received messages.
 */
void QGCMAVLinkInspector::clearView()
{
	// FIX: clear all maps!
	widgetTreeItems.clear();
	
	uasTreeWidgetItems.clear();
	uasMsgTreeItems.clear();
	msgUasTreeItems.clear();

	uasMavlinkStorage.clear();
	uasMessageHz.clear();
	uasMessageCount.clear();

	uasLastMessageUpdate.clear();

    ui->treeWidget->clear();
}

//void QGCMAVLinkInspector::refreshView()
//{
//
//	QMap<int, mavlink_message_t* >::const_iterator ite;
//
//	for(ite=uasMavlinkStorage.constBegin(); ite!=uasMavlinkStorage.constEnd();ite++)
//	{
//		for (int i = 0; i < 256; ++i)//mavlink_message_t msg, receivedMessages)
//		{
//			mavlink_message_t* msg2 = ite.value()+i;
//			if(msg2->msgid == 0xFF) continue;
//			qDebug() << QString::number(msg2->msgid);
//		}
//	}
//
//    for (int i = 0; i < 256; ++i)//mavlink_message_t msg, receivedMessages)
//    {
//        mavlink_message_t* msg = receivedMessages+i;
//		
//        // Ignore NULL values
//        if (msg->msgid == 0xFF) continue;
//        // Update the tree view
//        QString messageName("%1 (%2 Hz, #%3)");
//        float msgHz = (1.0f-updateHzLowpass)*messagesHz.value(msg->msgid, 0) + updateHzLowpass*((float)messageCount.value(msg->msgid, 0))/((float)updateInterval/1000.0f);
//        messagesHz.insert(msg->msgid, msgHz);
//        messageName = messageName.arg(messageInfo[msg->msgid].name).arg(msgHz, 3, 'f', 1).arg(msg->msgid);
//        messageCount.insert(msg->msgid, 0);
//
//		
//
//		if(!uasTreeWidgetItems.contains(msg->sysid))
//		{
//
//			UASInterface* uas = UASManager::instance()->getUASForId(msg->sysid);
//
//			QStringList idstring;
//			if (uas->getUASName() == "")
//			{
//				idstring << tr("UAS ") + QString::number(uas->getUASID());
//			}
//			else
//			{
//				idstring << uas->getUASName();
//			}
//
//			QTreeWidgetItem* uasWidget = new QTreeWidgetItem(idstring);
//			uasWidget->setFirstColumnSpanned(false);
//			uasTreeWidgetItems.insert(msg->sysid,uasWidget);
//
//			ui->treeWidget->addTopLevelItem(uasWidget);
//
//			uasMsgTreeItems.insert(msg->sysid,new QMap<int, QTreeWidgetItem*>());
//
//		}
//
//		QMap<int, QTreeWidgetItem*>* msgTreeItems = uasMsgTreeItems.value(msg->sysid);
//
//		if(!msgTreeItems->contains(msg->msgid))
//		{
//			QStringList fields;
//            fields << messageName;
//            QTreeWidgetItem* widget = new QTreeWidgetItem(fields);
//            widget->setFirstColumnSpanned(true);
//
//			widget->setFlags(widget->flags() | Qt::ItemIsUserCheckable);
//
//			for (unsigned int i = 0; i < messageInfo[msg->msgid].num_fields; ++i)
//            {
//                QTreeWidgetItem* field = new QTreeWidgetItem();
//                widget->addChild(field);
//            }
//
//			msgTreeItems->insert(msg->msgid,widget);
//			msgUasTreeItems.insert(widget,msg->sysid);
//			widgetTreeItems.insert(widget,msg->msgid);
//
//			QList<int> groupKeys = msgTreeItems->uniqueKeys();
//			int insertIndex = groupKeys.indexOf(msg->msgid);
//
//			uasTreeWidgetItems.value(msg->sysid)->insertChild(insertIndex,widget);
//		}
//
//		QTreeWidgetItem* message = msgTreeItems->value(msg->msgid);
//
//		if(message){
//			
//			if (msgHz <= 0.01)
//			{
//				message->setCheckState(0,Qt::Unchecked);
//			}else{
//				message->setCheckState(0,Qt::Checked);
//			}
//
//			message->setFirstColumnSpanned(true);
//			message->setData(0, Qt::DisplayRole, QVariant(messageName));
//			for (unsigned int i = 0; i < messageInfo[msg->msgid].num_fields; ++i)
//			{
//				updateField(msg->msgid, i, message->child(i));
//			}
//		}
//    }
//}

void QGCMAVLinkInspector::refreshView()
{

	QMap<int, mavlink_message_t* >::const_iterator ite;

	for(ite=uasMavlinkStorage.constBegin(); ite!=uasMavlinkStorage.constEnd();ite++)
	{
		for (int i = 0; i < 256; ++i)
		{
			mavlink_message_t* msg = ite.value()+i;
			
			// Ignore NULL values
			if (msg->msgid == 0xFF) continue;
			//qDebug() << "msg sysid n msgid:" << QString::number(msg->sysid) << QString::number(msg->msgid);

			// Update the tree view
			QString messageName("%1 (%2 Hz, #%3)");

			/*QMap<int, QTreeWidgetItem*>* msgTreeItems = uasMsgTreeItems.value(msg->sysid);
			if(!msgTreeItems->contains(msg->msgid))
			{
				if(msg->msgid == 0)
				{
					qDebug() << QString::number(msg->msgid);
				}
				QMap<int, float>* messageHz = new QMap<int,float>;
				messageHz->insert(msg->msgid,0.0f);
				uasMessageHz.insertMulti(msg->sysid,messageHz);

				QMap<int, unsigned int>* messagesCount = new QMap<int, unsigned int>;
				messagesCount->insert(msg->msgid,0);
				uasMessageCount.insertMulti(msg->sysid,messagesCount);

				QMap<int, quint64>* lastMessage = new QMap<int, quint64>;
				lastMessage->insert(msg->msgid,0.0f);
				uasLastMessageUpdate.insertMulti(msg->sysid,lastMessage);
			}*/

			float msgHz = 0.0f;
			QMap<int, QMap<int, float>* >::const_iterator iteHz = uasMessageHz.find(msg->sysid);
			QMap<int, float> * uasMessagesHz;
			while((iteHz != uasMessageHz.end()) && (iteHz.key() == msg->sysid))
			{
				if(iteHz.value()->contains(msg->msgid))
				{
					uasMessagesHz = iteHz.value();
					msgHz = iteHz.value()->value(msg->msgid);
					break;
				}
				++iteHz;
			}

			float msgCount = 0;
			QMap<int, unsigned int>* uasMsgCount;
			QMap<int, QMap<int, unsigned int> * >::const_iterator iter = uasMessageCount.find(msg->sysid);
			while((iter != uasMessageCount.end()) && (iter.key()==msg->sysid))
			{
				if(iter.value()->contains(msg->msgid))
				{
					msgCount = (float) iter.value()->value(msg->msgid);
					uasMsgCount = iter.value();
					break;
				}
				++iter;
			}


			msgHz = (1.0f-updateHzLowpass)* msgHz + updateHzLowpass*msgCount/((float)updateInterval/1000.0f);

			//qDebug() << "msg count n hz:" << QString::number(msgCount) << QString::number(msgHz);

			uasMessagesHz->insert(msg->msgid,msgHz);
			uasMsgCount->insert(msg->msgid,(unsigned int) 0);

			messageName = messageName.arg(messageInfo[msg->msgid].name).arg(msgHz, 3, 'f', 1).arg(msg->msgid);
        

			if(!uasTreeWidgetItems.contains(msg->sysid))
			{

				UASInterface* uas = UASManager::instance()->getUASForId(msg->sysid);

				QStringList idstring;
				if (uas->getUASName() == "")
				{
					idstring << tr("UAS ") + QString::number(uas->getUASID());
				}
				else
				{
					idstring << uas->getUASName();
				}

				QTreeWidgetItem* uasWidget = new QTreeWidgetItem(idstring);
				uasWidget->setFirstColumnSpanned(true);
				uasTreeWidgetItems.insert(msg->sysid,uasWidget);

				ui->treeWidget->addTopLevelItem(uasWidget);

				uasMsgTreeItems.insert(msg->sysid,new QMap<int, QTreeWidgetItem*>());

			}

			QMap<int, QTreeWidgetItem*>* msgTreeItems = uasMsgTreeItems.value(msg->sysid);

			if(!msgTreeItems->contains(msg->msgid))
			{
				QStringList fields;
				fields << messageName;
				//QTreeWidgetItem* widget = new QTreeWidgetItem(fields);
				QTreeWidgetItem* widget = new QTreeWidgetItem();
				//widget->setData(0,Qt::DisplayRole,QVariant(fields));

				widget->setFlags(widget->flags() | Qt::ItemIsUserCheckable);

				for (unsigned int i = 0; i < messageInfo[msg->msgid].num_fields; ++i)
				{
					QTreeWidgetItem* field = new QTreeWidgetItem();
					widget->addChild(field);
				}

				msgTreeItems->insert(msg->msgid,widget);
				msgUasTreeItems.insert(widget,msg->sysid);
				widgetTreeItems.insert(widget,msg->msgid);

				QList<int> groupKeys = msgTreeItems->uniqueKeys();
				int insertIndex = groupKeys.indexOf(msg->msgid);

				uasTreeWidgetItems.value(msg->sysid)->insertChild(insertIndex,widget);
			}

			QTreeWidgetItem* message = msgTreeItems->value(msg->msgid);

			if(message){
			
				if (msgHz <= 0.01)
				{
					message->setCheckState(0,Qt::Unchecked);
				}else{
					message->setCheckState(0,Qt::Checked);
				}

				message->setFirstColumnSpanned(true);
				message->setData(0, Qt::DisplayRole, QVariant(messageName));
				//message->setData(1, Qt::DisplayRole, "Freq1");
				//message->setData(2, Qt::DisplayRole, "Freq2");
				//message->setData(3, Qt::DisplayRole, "Freq3");
				for (unsigned int i = 0; i < messageInfo[msg->msgid].num_fields; ++i)
				{
					updateField(msg->sysid,msg->msgid, i, message->child(i));
				}
			}
		}
	}
}

void QGCMAVLinkInspector::addUAStoTree(int sysId)
{
	if(!uasTreeWidgetItems.contains(sysId))
	{
		UASInterface* uas = UASManager::instance()->getUASForId(sysId);

		QStringList idstring;
		if (uas->getUASName() == "")
		{
			idstring << tr("UAS ") + QString::number(uas->getUASID());
		}
		else
		{
			idstring << uas->getUASName();
		}

		QTreeWidgetItem* uasWidget = new QTreeWidgetItem(idstring);
		uasWidget->setFirstColumnSpanned(true);
		uasTreeWidgetItems.insert(sysId,uasWidget);

		ui->treeWidget->addTopLevelItem(uasWidget);

		uasMsgTreeItems.insert(sysId,new QMap<int, QTreeWidgetItem*>());
	}
}

QTreeWidgetItem* QGCMAVLinkInspector::findParentWidgetForStream(int sysId, int msgId, QString messageName)
{
	QTreeWidgetItem* parentWidget;
	QMap<int, QTreeWidgetItem*>* msgTreeItems = uasMsgTreeItems.value(sysId);

	if(!msgTreeItems->contains(msgId))
	{
		QStringList fields;
        fields << messageName;
        QTreeWidgetItem* widget = new QTreeWidgetItem(fields);
        widget->setFirstColumnSpanned(true);

		widget->setFlags(widget->flags() | Qt::ItemIsUserCheckable);

		for (unsigned int i = 0; i < messageInfo[msgId].num_fields; ++i)
        {
            QTreeWidgetItem* field = new QTreeWidgetItem();
            widget->addChild(field);
        }

		msgTreeItems->insert(msgId,widget);
		msgUasTreeItems.insert(widget,sysId);

		QList<int> groupKeys = msgTreeItems->uniqueKeys();
		int insertIndex = groupKeys.indexOf(msgId);

		uasTreeWidgetItems.value(sysId)->insertChild(insertIndex,widget);

		widgetTreeItems.insert(widget,msgId);

		parentWidget = msgTreeItems->value(msgId);
	}else{
		parentWidget = uasTreeWidgetItems.value(sysId);
	}
	return parentWidget;
}

QTreeWidgetItem* QGCMAVLinkInspector::findChildWidgetForStream(QTreeWidgetItem* parentItem, int msgId)
{
	QTreeWidgetItem* childItem = NULL;

	for (int i = 0; i < parentItem->childCount(); i++) {
        QTreeWidgetItem* child = parentItem->child(i);
        int key = child->data(0, Qt::DisplayRole).toInt();
        if (key == msgId)  {
            childItem = child;
            break;
        }
    }

    return childItem;
}

void QGCMAVLinkInspector::receiveMessage(LinkInterface* link,mavlink_message_t message)
{
    Q_UNUSED(link);
    /*if (selectedSystemID != 0 && selectedSystemID != message.sysid) return;
    if (selectedComponentID != 0 && selectedComponentID != message.compid) return;*/

	// Only overwrite if system filter is set
	quint64 receiveTime;

	if (!uasCreated)
	{
		return;
	}

	if(!uasMavlinkStorage.contains(message.sysid))
	{
		mavlink_message_t* msg = new mavlink_message_t[256];
		memset(msg,0xFF, sizeof(mavlink_message_t)*256);
		uasMavlinkStorage.insert(message.sysid,msg);

		addUAStoTree(message.sysid);
	}

	memcpy(uasMavlinkStorage.value(message.sysid)+message.msgid,&message,sizeof(mavlink_message_t));

	//qDebug() << "msg received:" <<QString::number(message.sysid) << QString::number(message.msgid);

	bool msgFound = false;
	QMap<int, quint64>* lastMessagesUpdate;
	QMap<int, QMap<int, quint64>* >::const_iterator ite = uasLastMessageUpdate.find(message.sysid);
	while((ite != uasLastMessageUpdate.end()) && (ite.key() == message.sysid))
	{		
		if(ite.value()->contains(message.msgid))
		{
			QMap<int, quint64>::const_iterator iterMap = ite.value()->find(message.msgid);
			int keyMsgid = iterMap.key();

			msgFound = true;
			lastMessagesUpdate = ite.value();
			break;
		}
		++ite;
	}

	receiveTime = QGC::groundTimeMilliseconds();
	if(!msgFound)
	{
		QMap<int, float>* messageHz = new QMap<int,float>;
		messageHz->insert(message.msgid,0.0f);
		uasMessageHz.insertMulti(message.sysid,messageHz);

		QMap<int, unsigned int>* messagesCount = new QMap<int, unsigned int>;
		messagesCount->insert(message.msgid,0);
		uasMessageCount.insertMulti(message.sysid,messagesCount);

		QMap<int, quint64>* lastMessage = new QMap<int, quint64>;
		lastMessage->insert(message.msgid,receiveTime);
		uasLastMessageUpdate.insertMulti(message.sysid,lastMessage);
		lastMessagesUpdate = lastMessage;

		msgFound = true;
	}
	if(msgFound)
	{

		if ((lastMessagesUpdate->contains(message.msgid))&&(uasMessageCount.contains(message.sysid)))
		{
			bool countFound = false;
			unsigned int count = 0;
			QMap<int, unsigned int> * uasMsgCount;
			QMap<int, QMap<int, unsigned int>* >::const_iterator iter = uasMessageCount.find(message.sysid);
			while((iter != uasMessageCount.end()) && (iter.key() == message.sysid))
			{

				if(iter.value()->contains(message.msgid))
				{
					QMap<int, unsigned int>::const_iterator iterMsgID = iter.value()->find(message.msgid);
					int keyMsgID = iterMsgID.key();

					countFound= true;
					uasMsgCount = iter.value();
					count = uasMsgCount->value(message.msgid,0);
					uasMsgCount->insert(message.msgid,count+1);
					break;
				}
				++iter;
			}
		}
		lastMessagesUpdate->insert(message.msgid,receiveTime);
	}
}

QGCMAVLinkInspector::~QGCMAVLinkInspector()
{
    delete ui;
}

void QGCMAVLinkInspector::updateField(int sysid, int msgid, int fieldid, QTreeWidgetItem* item)
{
    // Add field tree widget item
    item->setData(0, Qt::DisplayRole, QVariant(messageInfo[msgid].fields[fieldid].name));

	uint8_t* m = ((uint8_t*)(uasMavlinkStorage.value(sysid)+msgid))+8;

    switch (messageInfo[msgid].fields[fieldid].type)
    {
    case MAVLINK_TYPE_CHAR:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            char* str = (char*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            str[messageInfo[msgid].fields[fieldid].array_length-1] = '\0';
            QString string(str);
            item->setData(2, Qt::DisplayRole, "char");
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single char
            char b = *((char*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, QString("char[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, b);
        }
        break;
    case MAVLINK_TYPE_UINT8_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            uint8_t* nums = m+messageInfo[msgid].fields[fieldid].wire_offset;
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[fieldid].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("uint8_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            uint8_t u = *(m+messageInfo[msgid].fields[fieldid].wire_offset);
            item->setData(2, Qt::DisplayRole, "uint8_t");
            item->setData(1, Qt::DisplayRole, u);
        }
        break;
    case MAVLINK_TYPE_INT8_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            int8_t* nums = (int8_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("int8_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            int8_t n = *((int8_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "int8_t");
            item->setData(1, Qt::DisplayRole, n);
        }
        break;
    case MAVLINK_TYPE_UINT16_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            uint16_t* nums = (uint16_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("uint16_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            uint16_t n = *((uint16_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "uint16_t");
            item->setData(1, Qt::DisplayRole, n);
        }
        break;
    case MAVLINK_TYPE_INT16_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            int16_t* nums = (int16_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("int16_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            int16_t n = *((int16_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "int16_t");
            item->setData(1, Qt::DisplayRole, n);
        }
        break;
    case MAVLINK_TYPE_UINT32_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            uint32_t* nums = (uint32_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("uint32_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            float n = *((uint32_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "uint32_t");
            item->setData(1, Qt::DisplayRole, n);
        }
        break;
    case MAVLINK_TYPE_INT32_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            int32_t* nums = (int32_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("int32_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            int32_t n = *((int32_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "int32_t");
            item->setData(1, Qt::DisplayRole, n);
        }
        break;
    case MAVLINK_TYPE_FLOAT:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            float* nums = (float*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("float[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            float f = *((float*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "float");
            item->setData(1, Qt::DisplayRole, f);
        }
        break;
    case MAVLINK_TYPE_DOUBLE:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            double* nums = (double*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("double[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            double f = *((double*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "double");
            item->setData(1, Qt::DisplayRole, f);
        }
        break;
    case MAVLINK_TYPE_UINT64_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            uint64_t* nums = (uint64_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("uint64_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            uint64_t n = *((uint64_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "uint64_t");
            item->setData(1, Qt::DisplayRole, (quint64) n);
        }
        break;
    case MAVLINK_TYPE_INT64_T:
        if (messageInfo[msgid].fields[fieldid].array_length > 0)
        {
            int64_t* nums = (int64_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset);
            // Enforce null termination
            QString tmp("%1, ");
            QString string;
            for (unsigned int j = 0; j < messageInfo[msgid].fields[j].array_length; ++j)
            {
                string += tmp.arg(nums[j]);
            }
            item->setData(2, Qt::DisplayRole, QString("int64_t[%1]").arg(messageInfo[msgid].fields[fieldid].array_length));
            item->setData(1, Qt::DisplayRole, string);
        }
        else
        {
            // Single value
            int64_t n = *((int64_t*)(m+messageInfo[msgid].fields[fieldid].wire_offset));
            item->setData(2, Qt::DisplayRole, "int64_t");
            item->setData(1, Qt::DisplayRole, (qint64) n);
        }
        break;
    }
	item->setData(3,Qt::DisplayRole,"bla");
}

void QGCMAVLinkInspector::activateStream(QTreeWidgetItem* item, int col)
{
	//Q_UNUSED(col);
	qDebug() << "Checkvalue:" << QString::number(item->checkState(0)) << ", on column:" << QString::number(col);

	QMap<QTreeWidgetItem*, int>::const_iterator it = msgUasTreeItems.constFind(item);
	QMap<QTreeWidgetItem*, int>::const_iterator it2 = widgetTreeItems.constFind(item);

	int sysID = it.value();
	int msgID = it2.value();

	mavlink_request_data_stream_t request_stream;
	mavlink_message_t msg;
	request_stream.target_system = sysID;
	request_stream.target_component = 0;
	if (item->checkState(0) == Qt::Checked)
	{
		qDebug() << "Switching on the clicked stream.";
		request_stream.start_stop = 1;
		request_stream.req_message_rate = ui->frequencySpinBox->value();
	}else{
		qDebug() << "Switching off the clicked stream.";
		request_stream.start_stop = 0;
		request_stream.req_message_rate = 0;
	}

	request_stream.req_stream_id = msgID; // request stream ID
		
	mavlink_msg_request_data_stream_encode(mavlink_protocol->getSystemId(),mavlink_protocol->getComponentId(), &msg, &request_stream);
	mavlink_protocol->sendMessage(msg);

	// setting messageCount and messageHz to zero
	QMap<int, QMap<int, float>* >::const_iterator iteHz = uasMessageHz.find(sysID);
	QMap<int, float> * uasMessagesHz;
	while((iteHz != uasMessageHz.end()) && (iteHz.key() == sysID))
	{
		if(iteHz.value()->contains(msgID))
		{
			uasMessagesHz = iteHz.value();
			break;
		}
		++iteHz;
	}
	QMap<int, unsigned int>* uasMsgCount;
	QMap<int, QMap<int, unsigned int> * >::const_iterator iter = uasMessageCount.find(sysID);
	while((iter != uasMessageCount.end()) && (iter.key()==sysID))
	{
		if(iter.value()->contains(msgID))
		{
			uasMsgCount = iter.value();
			break;
		}
		++iter;
	}
	uasMessagesHz->insert(msgID,0.0);
	uasMsgCount->insert(msgID,(unsigned int) 0);
}

void QGCMAVLinkInspector::sendStreamButton()
{
	UASInterface* uas = UASManager::instance()->getActiveUAS();
	mavlink_request_data_stream_t request_stream;
	mavlink_message_t msg;
	
	request_stream.target_system = uas->getUASID();
	request_stream.target_component = 0; //TODO: change to uas->getCompenentId();
	request_stream.req_message_rate = 1;
	request_stream.req_stream_id = 255; // request all streams
	request_stream.start_stop = 1;
	
	mavlink_msg_request_data_stream_encode(mavlink_protocol->getSystemId(),mavlink_protocol->getComponentId(), &msg, &request_stream);
	mavlink_protocol->sendMessage(msg);


}