#include "slater_low.h"

cl_R DSlater_1S_2S(cl_R r,cl_R xi,cl_R xj)
{
  cl_R S,rxi,rxj;

  rxi = rxj = S = ZERO;
  rxi = r*xi;
  rxj = r*xj;
  if (xi == xj) {
        if (r == 0LL) {  S = 0LL

    ; } else {  S = -(-375LL*xi + 480LL*exp(2LL*r*xi)*xi - 540LL*r*Power(xi,2LL) - 

          345LL*Power(r,2LL)*Power(xi,3LL) - 120LL*Power(r,3LL)*Power(xi,4LL) - 

          20LL*Power(r,4LL)*Power(xi,5LL))/(240LL*exp(2LL*r*xi)*r) + 

      (-240LL + 240LL*exp(2LL*r*xi) - 375LL*r*xi - 270LL*Power(r,2LL)*Power(xi,2LL) - 

         115LL*Power(r,3LL)*Power(xi,3LL) - 30LL*Power(r,4LL)*Power(xi,4LL) - 

         4LL*Power(r,5LL)*Power(xi,5LL))/(240LL*exp(2LL*r*xi)*Power(r,2LL)) + 

      (xi*(-240LL + 240LL*exp(2LL*r*xi) - 375LL*r*xi - 270LL*Power(r,2LL)*Power(xi,2LL) - 

           115LL*Power(r,3LL)*Power(xi,3LL) - 30LL*Power(r,4LL)*Power(xi,4LL) - 

           4LL*Power(r,5LL)*Power(xi,5LL)))/(120LL*exp(2LL*r*xi)*r)

    ; }
 
  }
  else {
      if (r == 0LL) {  S = 0LL

    ; } else {  S = (6LL*exp(2LL*r*(xi + xj))*Power(Power(xi,2LL) - Power(xj,2LL),5LL) + 

         6LL*exp(2LL*r*xj)*Power(xj,6LL)*

          (-4LL*Power(xi,4LL) - r*Power(xi,5LL) - 5LL*Power(xi,2LL)*Power(xj,2LL) + 

            Power(xj,4LL) + r*xi*Power(xj,4LL)) - 

         exp(2LL*r*xi)*Power(xi,4LL)*

          (Power(xi,6LL)*(6LL + 9LL*r*xj + 6LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            3LL*Power(xi,4LL)*Power(xj,2LL)*

             (10LL + 15LL*r*xj + 10LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) + 

            3LL*Power(xi,2LL)*Power(xj,4LL)*

             (20LL + 33LL*r*xj + 14LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            Power(xj,6LL)*(84LL + 63LL*r*xj + 18LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL))))/

       (6LL*exp(2LL*r*(xi + xj))*Power(r,2LL)*Power(xi - xj,5LL)*Power(xi + xj,5LL)) + 

      (6LL*exp(2LL*r*(xi + xj))*Power(Power(xi,2LL) - Power(xj,2LL),5LL) + 

         6LL*exp(2LL*r*xj)*Power(xj,6LL)*

          (-4LL*Power(xi,4LL) - r*Power(xi,5LL) - 5LL*Power(xi,2LL)*Power(xj,2LL) + 

            Power(xj,4LL) + r*xi*Power(xj,4LL)) - 

         exp(2LL*r*xi)*Power(xi,4LL)*

          (Power(xi,6LL)*(6LL + 9LL*r*xj + 6LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            3LL*Power(xi,4LL)*Power(xj,2LL)*

             (10LL + 15LL*r*xj + 10LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) + 

            3LL*Power(xi,2LL)*Power(xj,4LL)*

             (20LL + 33LL*r*xj + 14LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            Power(xj,6LL)*(84LL + 63LL*r*xj + 18LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL))))/

       (3LL*exp(2LL*r*(xi + xj))*r*Power(xi - xj,5LL)*Power(xi + xj,4LL)) - 

      (12LL*exp(2LL*r*(xi + xj))*(xi + xj)*Power(Power(xi,2LL) - Power(xj,2LL),5LL) + 

         6LL*exp(2LL*r*xj)*Power(xj,6LL)*(-Power(xi,5LL) + xi*Power(xj,4LL)) + 

         12LL*exp(2LL*r*xj)*Power(xj,7LL)*

          (-4LL*Power(xi,4LL) - r*Power(xi,5LL) - 5LL*Power(xi,2LL)*Power(xj,2LL) + 

            Power(xj,4LL) + r*xi*Power(xj,4LL)) - 

         exp(2LL*r*xi)*Power(xi,4LL)*

          (Power(xi,6LL)*(9LL*xj + 12LL*r*Power(xj,2LL) + 6LL*Power(r,2LL)*Power(xj,3LL)) - 

            3LL*Power(xi,4LL)*Power(xj,2LL)*

             (15LL*xj + 20LL*r*Power(xj,2LL) + 6LL*Power(r,2LL)*Power(xj,3LL)) + 

            3LL*Power(xi,2LL)*Power(xj,4LL)*

             (33LL*xj + 28LL*r*Power(xj,2LL) + 6LL*Power(r,2LL)*Power(xj,3LL)) - 

            Power(xj,6LL)*(63LL*xj + 36LL*r*Power(xj,2LL) + 6LL*Power(r,2LL)*Power(xj,3LL))) - 

         2LL*exp(2LL*r*xi)*Power(xi,5LL)*

          (Power(xi,6LL)*(6LL + 9LL*r*xj + 6LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            3LL*Power(xi,4LL)*Power(xj,2LL)*

             (10LL + 15LL*r*xj + 10LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) + 

            3LL*Power(xi,2LL)*Power(xj,4LL)*

             (20LL + 33LL*r*xj + 14LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL)) - 

            Power(xj,6LL)*(84LL + 63LL*r*xj + 18LL*Power(r,2LL)*Power(xj,2LL) + 

               2LL*Power(r,3LL)*Power(xj,3LL))))/

       (6LL*exp(2LL*r*(xi + xj))*r*Power(xi - xj,5LL)*Power(xi + xj,5LL))

    ; }
   
  }
  return S;
}


cl_R DSlater_2S_1S(cl_R r,cl_R xi,cl_R xj)
{
  return DSlater_1S_2S(r,xj,xi);
}

