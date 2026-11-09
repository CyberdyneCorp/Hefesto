// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The URLSession Transport (macOS; hefesto-licensing D-13): a thin
// Objective-C++ bridge, compiled with ARC, whose Foundation types stay in
// this file. One ephemeral session per transfer (no cookies, cache or
// credentials store), a data task streamed through a delegate on a serial
// queue, and the calling thread blocked until the task completes.
// URLSession validates certificates against the system trust store.

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "hefesto/platform_net/http_fetcher.hpp"
#include "hefesto/platform_net/transport.hpp"
#include "hefesto/version.hpp"

namespace hp = hefesto::platform_net;

namespace {

constexpr NSInteger kMaxRedirects = 10;
// URLSession's resource timeout when the request sets none (its own default).
constexpr NSTimeInterval kNoTotalTimeout = 7.0 * 24.0 * 3600.0;

hp::TransferError classify(NSError* error) {
  if (error == nil) return hp::TransferError::none;
  if (![error.domain isEqualToString:NSURLErrorDomain]) return hp::TransferError::other;
  switch (error.code) {
    case NSURLErrorTimedOut: return hp::TransferError::timeout;
    case NSURLErrorCannotFindHost:
    case NSURLErrorDNSLookupFailed:
    case NSURLErrorNotConnectedToInternet: return hp::TransferError::resolve;
    case NSURLErrorCannotConnectToHost: return hp::TransferError::connect;
    case NSURLErrorNetworkConnectionLost:
    case NSURLErrorZeroByteResource: return hp::TransferError::interrupted;
    case NSURLErrorCancelled: return hp::TransferError::aborted;
    default: return hp::TransferError::other;
  }
}

NSString* to_ns(const std::string& text) {
  return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

bool scheme_allowed(NSURL* url, bool allow_plain_http) {
  NSString* scheme = url.scheme.lowercaseString;
  return [scheme isEqualToString:@"https"] || (allow_plain_http && [scheme isEqualToString:@"http"]);
}

}  // namespace

// Streams the final response of one data task to a TransferSink.
@interface HefestoTransferDelegate : NSObject <NSURLSessionDataDelegate>
- (instancetype)initWithSink:(hp::TransferSink*)sink host:(NSString*)host plainHttp:(bool)plainHttp;
@property(nonatomic, readonly) int status;
@property(nonatomic, readonly) bool aborted;
@property(nonatomic, readonly, copy) NSString* refusal;  // why a redirect was refused
@property(nonatomic, readonly, strong) NSError* error;
- (void)wait;
@end

@implementation HefestoTransferDelegate {
  hp::TransferSink* sink_;
  NSString* host_;
  bool plainHttp_;
  NSInteger redirects_;
  dispatch_semaphore_t done_;
}

- (instancetype)initWithSink:(hp::TransferSink*)sink host:(NSString*)host plainHttp:(bool)plainHttp {
  if ((self = [super init])) {
    sink_ = sink;
    host_ = [host copy];
    plainHttp_ = plainHttp;
    done_ = dispatch_semaphore_create(0);
  }
  return self;
}

- (void)wait {
  dispatch_semaphore_wait(done_, DISPATCH_TIME_FOREVER);
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
  (void)session;
  (void)task;
  (void)response;
  if (++redirects_ > kMaxRedirects) {
    _refusal = @"too many redirects";
  } else if (!scheme_allowed(request.URL, plainHttp_)) {
    _refusal = [@"redirect to an unsupported protocol: " stringByAppendingString:request.URL.absoluteString];
  }
  if (_refusal != nil) {
    completionHandler(nil);
    [task cancel];
    return;
  }
  NSMutableURLRequest* next = [request mutableCopy];
  if (![request.URL.host isEqualToString:host_]) {
    [next setValue:nil forHTTPHeaderField:@"Authorization"];  // never sent to another host
  }
  completionHandler(next);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
  (void)session;
  (void)dataTask;
  hp::TransferHead head;
  if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
    id any = response;
    NSHTTPURLResponse* http = any;
    head.status = static_cast<int>(http.statusCode);
  }
  if (response.expectedContentLength >= 0) head.length = static_cast<std::uint64_t>(response.expectedContentLength);
  _status = head.status;
  _aborted = !sink_->head(head);
  completionHandler(_aborted ? NSURLSessionResponseCancel : NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data {
  (void)session;
  if (_aborted) return;
  [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
    if (!sink_->body(std::string_view{static_cast<const char*>(bytes), range.length})) {
      _aborted = true;
      *stop = YES;
    }
  }];
  if (_aborted) [dataTask cancel];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error {
  (void)session;
  (void)task;
  _error = error;
  dispatch_semaphore_signal(done_);
}

@end

namespace hefesto::platform_net {

namespace {

NSURLSession* make_session(const TransferRequest& request, HefestoTransferDelegate* delegate) {
  NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
  config.timeoutIntervalForRequest = static_cast<NSTimeInterval>(request.stall_timeout.count());
  config.timeoutIntervalForResource =
      request.total_timeout.count() > 0 ? static_cast<NSTimeInterval>(request.total_timeout.count()) : kNoTotalTimeout;
  config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
  config.URLCache = nil;
  config.HTTPCookieStorage = nil;
  config.HTTPShouldSetCookies = NO;
  config.URLCredentialStorage = nil;
  NSOperationQueue* queue = [[NSOperationQueue alloc] init];
  queue.maxConcurrentOperationCount = 1;  // the sink is never called concurrently
  return [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:queue];
}

NSMutableURLRequest* make_request(const TransferRequest& request, NSURL* url) {
  NSMutableURLRequest* out = [NSMutableURLRequest requestWithURL:url];
  [out setValue:to_ns("Hefesto/" + std::string{version()}) forHTTPHeaderField:@"User-Agent"];
  [out setValue:@"identity" forHTTPHeaderField:@"Accept-Encoding"];  // byte ranges of the stored bytes
  for (const auto& [name, value] : request.headers) {
    [out setValue:to_ns(value) forHTTPHeaderField:to_ns(name)];
  }
  if (request.range_from > 0) {
    [out setValue:to_ns("bytes=" + std::to_string(request.range_from) + "-") forHTTPHeaderField:@"Range"];
  }
  return out;
}

TransferResult result_of(HefestoTransferDelegate* delegate) {
  TransferResult result{delegate.status, classify(delegate.error), {}};
  if (delegate.refusal != nil) {
    result.error = TransferError::other;
    result.message = delegate.refusal.UTF8String;
  } else if (delegate.aborted) {
    result.error = TransferError::aborted;
  } else if (delegate.error != nil) {
    result.message = delegate.error.localizedDescription.UTF8String;
  }
  return result;
}

class UrlSessionTransport final : public Transport {
 public:
  TransferResult perform(const TransferRequest& request, TransferSink& sink) override {
    @autoreleasepool {
      NSURL* url = [NSURL URLWithString:to_ns(request.url)];
      if (url == nil || !scheme_allowed(url, request.allow_plain_http)) {
        return {0, TransferError::other, "unsupported URL"};
      }
      auto* delegate = [[HefestoTransferDelegate alloc] initWithSink:&sink
                                                                 host:url.host
                                                            plainHttp:request.allow_plain_http];
      NSURLSession* session = make_session(request, delegate);
      [[session dataTaskWithRequest:make_request(request, url)] resume];
      [delegate wait];
      [session finishTasksAndInvalidate];  // releases the delegate
      TransferResult result = result_of(delegate);
      if (result.error == TransferError::none && delegate.status == 0) {
        result.error = TransferError::other;
        result.message = "no HTTP response";
      }
      return result;
    }
  }
};

}  // namespace

std::unique_ptr<Transport> make_system_transport() {
  return std::make_unique<UrlSessionTransport>();
}

std::string_view http_backend_name() noexcept {
  return "URLSession";
}

}  // namespace hefesto::platform_net
