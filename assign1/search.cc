#include "imdb-utils.h"
#include "imdb.h"
#include "path.h"
#include <iostream>
#include <list>
#include <set>
#include <vector>
using namespace std;

/**
* Method: findShortestPath
--------------------------
to find the shortest path between start player and end player using bfs
* @param startPlayer the begin of the path
* @param endPlayer the end of the path
* @param imdb the database to retrieve
* @return the path from startPlayer to endPlayer
*/
path findShortestPath(const string &startPlayer, const string &endPlayer,
                      const imdb &db) {
  list<path> queue;
  set<string> seenPlayers;
  set<film> seenFilms;

  // 初始化
  path initialPath(startPlayer);
  queue.push_back(initialPath);
  seenPlayers.insert(startPlayer); // 建议的修改：立即标记起点

  // BFS 主循环
  while (!queue.empty()) {
    path currentPath = queue.front();
    queue.pop_front();

    string currentPlayer = currentPath.getLastPlayer();

    // 在取出时检查是否到达终点，这是BFS的标准做法
    if (currentPlayer == endPlayer) {
      return currentPath;
    }

    // 获取当前玩家的所有电影
    vector<film> credits;
    if (db.getCredits(currentPlayer, credits)) {
      for (const film &credit : credits) {
        // 优化：如果电影处理过，就跳过
        if (seenFilms.count(credit)) {
          continue;
        }
        seenFilms.insert(credit);

        // 获取电影的所有演员
        vector<string> cast;
        if (db.getCast(credit, cast)) {
          for (const string &costar : cast) {
            // 如果演员未被访问过
            if (seenPlayers.find(costar) == seenPlayers.end()) {
              seenPlayers.insert(costar);

              // 关键修复：创建全新的路径副本
              path newPath = currentPath;
              newPath.addConnection(credit, costar);

              // 将新路径放入队列
              queue.push_back(newPath);
            }
          }
        }
      }
    }
  }

  // 队列为空，说明没有找到路径
  return path("");
}

int main(int argc, char *argv[]) {
  // sanity check
  if (argc != 3) {
    fprintf(stderr, "Usage %s <starterPlayer> <endPlayer>\n", argv[0]);
    return 1;
  }
  imdb imdb{kIMDBDataDirectory};
  path path = findShortestPath(argv[1], argv[2], imdb);
  if (path.getLastPlayer() == "") {
    cout << "No Path found" << endl;
  } else {
    cout << path << endl;
  }
  return 0;
}
