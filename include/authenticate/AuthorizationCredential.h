#ifndef FRAMEWORK_AUTHORIZATIONCREDENTIAL_H__
#define FRAMEWORK_AUTHORIZATIONCREDENTIAL_H__

#include <string>

struct AuthorizationCredential
{
private:
  std::string accessToken_;
  std::string refreshToken_;
};

#endif // FRAMEWORK_AUTHORIZATIONCREDENTIAL_H__