namespace cpp media_service
namespace py media_service
namespace lua media_service

struct User {
    1: i64 user_id;
    2: string first_name;
    3: string last_name;
    4: string username;
    5: string password;
    6: string salt;
}

struct Review {
    1: i64 review_id;
    2: i64 user_id;
    3: i64 req_id;
    4: string text;
    5: string movie_id;
    6: i32 rating;
    7: i64 timestamp;
}

enum ErrorCode {
  SE_THRIFT_CONNPOOL_TIMEOUT,
  SE_THRIFT_CONN_ERROR,
  SE_UNAUTHORIZED,
  SE_MEMCACHED_ERROR,
  SE_MONGODB_ERROR,
  SE_REDIS_ERROR,
  SE_THRIFT_HANDLER_ERROR
}

struct CastInfo {
  1: i64 cast_info_id
  2: string name
  3: bool gender
  4: string intro
}

struct Cast {
  1: i32 cast_id
  2: string character
  3: i64 cast_info_id
}

struct MovieInfo {
  1: string movie_id,
  2: string title,
  3: list<Cast> casts
  4: i64 plot_id
  5: list<string> thumbnail_ids
  6: list<string> photo_ids
  7: list<string> video_ids
  8: double avg_rating
  9: i32 num_rating
}

struct Page {
  1: MovieInfo movie_info
  2: list<Review> reviews
  3: list<CastInfo> cast_infos
  4: string plot
}

struct BaseRpcResponse {
  1: string baggage;
}

struct LoginRpcResponse {
  1: string baggage;
  2: string result;
}

struct ReviewListRpcResponse {
  1: string baggage;
  2: list<Review> result;
}

struct CastInfoListRpcResponse {
  1: string baggage;
  2: list<CastInfo> result;
}

struct PlotRpcResponse {
  1: string baggage;
  2: string result;
}

struct MovieInfoRpcResponse {
  1: string baggage;
  2: MovieInfo result;
}

struct PageRpcResponse {
  1: string baggage;
  2: Page result;
}

exception ServiceException {
  1: ErrorCode errorCode;
  2: string message;
}

service UniqueIdService {
  BaseRpcResponse UploadUniqueId (
      1: i64 req_id,
      2: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service MovieIdService {
  BaseRpcResponse UploadMovieId(
      1: i64 req_id,
      2: string title,
      3: i32 rating,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse RegisterMovieId(
      1: i64 req_id,
      2: string title,
      3: string movie_id,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service TextService {
  BaseRpcResponse UploadText (
      1: i64 req_id,
      2: string text,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service RatingService {
  BaseRpcResponse UploadRating (
      1: i64 req_id,
      2: string movie_id,
      3: i32 rating,
      4: map<string, string> carrier
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

  BaseRpcResponse UploadUserWithUserId(
      1: i64 req_id,
      2: i64 user_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UploadUserWithUsername(
      1: i64 req_id,
      2: string username,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service ComposeReviewService {
  BaseRpcResponse UploadText(
      1: i64 req_id,
      2: string text,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
  BaseRpcResponse UploadRating(
      1: i64 req_id,
      2: i32 rating,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
  BaseRpcResponse UploadMovieId(
      1: i64 req_id,
      2: string movie_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
  BaseRpcResponse UploadUniqueId(
      1: i64 req_id,
      2: i64 unique_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
  BaseRpcResponse UploadUserId(
      1: i64 req_id,
      2: i64 user_id,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service ReviewStorageService {
  BaseRpcResponse StoreReview(
      1: i64 req_id,
      2: Review review,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  ReviewListRpcResponse ReadReviews(
      1: i64 req_id,
      2: list<i64> review_ids
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service MovieReviewService {
  BaseRpcResponse UploadMovieReview(
      1: i64 req_id,
      2: string movie_id,
      3: i64 review_id,
      4: i64 timestamp,
      5: map<string, string> carrier
  ) throws (1: ServiceException se)

  ReviewListRpcResponse ReadMovieReviews(
      1: i64 req_id,
      2: string movie_id,
      3: i32 start,
      4: i32 stop,
      5: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service UserReviewService {
  BaseRpcResponse UploadUserReview(
      1: i64 req_id,
      2: i64 user_id,
      3: i64 review_id,
      4: i64 timestamp,
      5: map<string, string> carrier
  ) throws (1: ServiceException se)

  ReviewListRpcResponse ReadUserReviews(
      1: i64 req_id,
      2: i64 user_id,
      3: i32 start,
      4: i32 stop,
      5: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service CastInfoService {
  BaseRpcResponse WriteCastInfo(
      1: i64 req_id,
      2: i64 cast_info_id,
      3: string name,
      4: bool gender,
      5: string intro,
      6: map<string, string> carrier
  ) throws (1: ServiceException se)

  CastInfoListRpcResponse ReadCastInfo(
      1: i64 req_id,
      2: list<i64> cast_ids,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service PlotService {
  BaseRpcResponse WritePlot(
      1: i64 req_id,
      2: i64 plot_id,
      3: string plot,
      4: map<string, string> carrier
  ) throws (1: ServiceException se)

  PlotRpcResponse ReadPlot(
      1: i64 req_id,
      2: i64 plot_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service MovieInfoService {
  BaseRpcResponse WriteMovieInfo(
    1: i64 req_id,
    2: string movie_id,
    3: string title,
    4: list<Cast> casts
    5: i64 plot_id
    6: list<string> thumbnail_ids
    7: list<string> photo_ids
    8: list<string> video_ids
    9: string avg_rating
    10: i32 num_rating
    11: map<string, string> carrier
  ) throws (1: ServiceException se)

  MovieInfoRpcResponse ReadMovieInfo(
      1: i64 req_id,
      2: string movie_id,
      3: map<string, string> carrier
  ) throws (1: ServiceException se)

  BaseRpcResponse UpdateRating(
    1: i64 req_id,
    2: string movie_id,
    3: i32 sum_uncommitted_rating
    4: i32 num_uncommitted_rating
    5: map<string, string> carrier
  ) throws (1: ServiceException se)
}

service PageService {
  PageRpcResponse ReadPage(
    1: i64 req_id,
    2: string movie_id,
    3: i32 review_start
    4: i32 review_stop
    5: map<string, string> carrier
  ) throws (1: ServiceException se)
}
