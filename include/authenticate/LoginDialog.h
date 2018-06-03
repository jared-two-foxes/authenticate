#ifndef FRAMEWORK_LOGINDIALOG_HPP__
#define FRAMEWORK_LOGINDIALOG_HPP__

#include <string>

class LoginDialog
{
public:
    LoginDialog();

    void setLoginUrl(const std::string& url);
    std::string authorizationCode();

    void authorizationCodeObtained();
    void urlChanged(const std::string& url);

private:
    std::string m_authorizationCode;

};

#endif // FRAMEWORK_LOGINDIALOG_HPP__
