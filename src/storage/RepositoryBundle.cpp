#include "storage/RepositoryBundle.h"
#include "storage/sql/SqlConnectionPool.h"

namespace storage{
    void RepositoryBundle::shutdown(){
        if(messageSqlPool){
            messageSqlPool->stop();
        }
        if(sqlPool){
            sqlPool->stop();
        }

    }
}