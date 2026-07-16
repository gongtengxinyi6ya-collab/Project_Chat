#include "storage/RepositoryBundle.h"
#include "storage/sql/SqlConnectionPool.h"

namespace storage{
    void RepositoryBundle::shutdown(){
        if(sqlPool){
            sqlPool->stop();
        }
        if(messageSqlPool){
            messageSqlPool->stop();
        }
    }
}