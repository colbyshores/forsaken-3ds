#ifdef LUA_BOT
#ifdef __cplusplus
extern "C" {
#endif
VECTOR BOTAI_NodeNetwork(int node);
VECTOR BOTAI_NodeNetworkAlpha(int node);
VECTOR BOTAI_NodeNetworkFourball(int node);
VECTOR BOTAI_NodeNetworkShip(int node);

int BOTAI_GetNearestNode(OBJECT * TObj);
int BOTAI_GetNodeGroup(int node);
void BOTAI_FollowNodeNetwork( bool SlideOnly, bool FollowingTarget );
void BOTAI_GetNextNode();
void BOTAI_GetNextNodeAlpha();
void BOTAI_GetNextNodeFourball();
void BOTAI_GetNextNodeShip();
#ifdef __cplusplus
}
#endif
#endif // LUA_BOT
