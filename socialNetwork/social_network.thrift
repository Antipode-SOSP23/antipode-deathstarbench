namespace cpp social_network
namespace py social_network
namespace lua social_network

struct User {
    1: i64 user_id;
    2: string first_name;
    3: string last_name;
    4: string username;
    5: string password_hashed;
    6: string salt;
}

enum ErrorCode {
  SE_CONNPOOL_TIMEOUT,
  SE_THRIFT_CONN_ERROR,
  SE_UNAUTHORIZED,
  SE_MEMCACHED_ERROR,
  SE_MONGODB_ERROR,
  SE_REDIS_ERROR,
  SE_THRIFT_HANDLER_ERROR,
  SE_RABBITMQ_CONN_ERROR
}

exception ServiceException {
    1: ErrorCode errorCode;
    2: string message;
}

enum PostType {
  POST,
  REPOST,
  REPLY,
  DM
}

struct Media {
  1: i64 media_id;
  2: string media_type;
}

struct Url {
  1: string shortened_url;
  2: string expanded_url;
}

struct UserMention {
  1: i64 user_id;
  2: string username;
}

struct Creator {
  1: i64 user_id;
  2: string username;
}

struct Post {
  1: i64 post_id;
  2: Creator creator;
  3: i64 req_id;
  4: string text;
  5: list<UserMention> user_mentions;
  6: list<Media> media;
  7: list<Url> urls;
  8: i64 timestamp;
  9: PostType post_type;
}

struct BaseRpcResponse {
  1: string baggage;
}

struct LoginRpcResponse {
  1: string baggage;
  2: string result;
}

struct UserIdRpcResponse {
  1: string baggage;
  2: i64 result;
}

struct PostRpcResponse {
  1: string baggage;
  2: Post result;
}

struct PostListRpcResponse {
  1: string baggage;
  2: list<Post> result;
}

struct UidListRpcResponse {
  1: string baggage;
  2: list<i64> result;
}

struct UrlListRpcResponse {
  1: string baggage;
  2: list<string> result;
}

service UniqueIdService {
  LoginRpcResponse UploadUniqueId (
      1: i64 req_id,
      2: PostType post_type,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service TextService {
  BaseRpcResponse UploadText (
      1: i64 req_id,
      2: string text,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service UserService {
  BaseRpcResponse RegisterUser (
      1: i64 req_id,
      2: string first_name,
      3: string last_name,
      4: string username,
      5: string password,
      6: map<string, string> carrier
  ) throws (1: ServiceException se)

    BaseRpcResponse RegisterUserWithId (
        1: i64 req_id,
        2: string first_name,
        3: string last_name,
        4: string username,
        5: string password,
        6: i64 user_id,
        7: map<string, string> carrier
    ) throws (1: ServiceException se)

  LoginRpcResponse Login(
      1: i64 req_id,
      2: string username,
      3: string password,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadCreatorWithUserId(
      1: i64 req_id,
      2: i64 user_id,
      3: string username,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadCreatorWithUsername(
      1: i64 req_id,
      2: string username,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  UserIdRpcResponse GetUserId(
      1: i64 req_id,
      2: string username,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service ComposePostService {
  BaseRpcResponse UploadText(
      1: i64 req_id,
      2: string text,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadMedia(
      1: i64 req_id,
      2: list<Media> media,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadUniqueId(
      1: i64 req_id,
      2: i64 post_id,
      3: PostType post_type
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadCreator(
      1: i64 req_id,
      2: Creator creator,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadUrls(
      1: i64 req_id,
      2: list<Url> urls,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadUserMentions(
      1: i64 req_id,
      2: list<UserMention> user_mentions,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service PostStorageService {
  BaseRpcResponse StorePost(
    1: i64 req_id,
    2: Post post,
    3: map<string, string> carrier
  ) throws (1: ServiceException se)

  PostRpcResponse ReadPost(
    1: i64 req_id,
    2: i64 post_id,
    3: map<string, string> carrier
  ) throws (1: ServiceException se)

  PostListRpcResponse ReadPosts(
    1: i64 req_id,
    2: list<i64> post_ids,
    3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service HomeTimelineService {
  PostListRpcResponse ReadHomeTimeline(
    1: i64 req_id,
    2: i64 user_id,
    3: i32 start,
    4: i32 stop,
    5: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service UserTimelineService {
  BaseRpcResponse WriteUserTimeline(
    1: i64 req_id,
    2: i64 post_id,
    3: i64 user_id,
    4: i64 timestamp,
    5: map<string, string> carrier
  ) throws (1: ServiceException se)

  PostListRpcResponse ReadUserTimeline(
    1: i64 req_id,
    2: i64 user_id,
    3: i32 start,
    4: i32 stop,
    5: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service SocialGraphService{
  UidListRpcResponse GetFollowers(
      1: i64 req_id,
      2: i64 user_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  UidListRpcResponse GetFollowees(
      1: i64 req_id,
      2: i64 user_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse Follow(
      1: i64 req_id,
      2: i64 user_id,
      3: i64 followee_id,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse Unfollow(
      1: i64 req_id,
      2: i64 user_id,
      3: i64 followee_id,
      4: map<string, string> carrier
   ) throws (1: ServiceException se)

  BaseRpcResponse FollowWithUsername(
      1: i64 req_id,
      2: string user_usernmae,
      3: string followee_username,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UnfollowWithUsername(
      1: i64 req_id,
      2: string user_usernmae,
      3: string followee_username,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse InsertUser(
     1: i64 req_id,
     2: i64 user_id,
     3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service UserMentionService {
  BaseRpcResponse UploadUserMentions(
      1: i64 req_id,
      2: list<string> usernames,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service UrlShortenService {
  UrlListRpcResponse UploadUrls(
      1: i64 req_id,
      2: list<string> urls,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  UrlListRpcResponse GetExtendedUrls(
      1: i64 req_id,
      2: list<string> shortened_urls,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service MediaService {
  BaseRpcResponse UploadMedia(
      1: i64 req_id,
      2: list<string> media_types,
      3: list<i64> media_ids,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service AntipodeOracle {
  bool MakeVisible(
      1: i64 object_id
      2: map<string, string> carrier
  ) throws (1: ServiceException se)

  bool IsVisible(
      1: i64 object_id
      2: map<string, string> carrier
  ) throws (1: ServiceException se)
}
