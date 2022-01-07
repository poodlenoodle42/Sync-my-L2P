#ifndef URLS
#define URLS

#include <QString>

const QString moodleApiDomain = "https://moped.ecampus.rwth-aachen.de/";
const QString moodleApiUrlBase = moodleApiDomain + "proxy/api/v2/";
const QString moodleApiDocs = moodleApiUrlBase + "Documentation";
const QString moodleApiUrl = moodleApiUrlBase + "eLearning/Moodle/";
const QString moodleGetMyEnrolledCoursesUrl = moodleApiUrl + "getmyenrolledcourses";
const QString moodleGetFilesUrl = moodleApiUrl + "getfiles";
const QString moodleDownloadFileUrl = moodleApiUrl + "downloadfile";

#endif // URLS

